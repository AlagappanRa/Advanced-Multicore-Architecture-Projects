#ifndef DRAGON_PROTOCOL_H
#define DRAGON_PROTOCOL_H

#include "coherence_defs.h"
#include <vector>
#include <unordered_map>
#include <list>
#include <iostream>
#include <iomanip>
#include <optional>

namespace Dragon {

    enum class DragonState { INVALID, SHARED_CLEAN, SHARED_MODIFIED, EXCLUSIVE, MODIFIED };

    struct CacheLine {
        uint32_t tag;
        DragonState state = DragonState::INVALID;
    };

    class LRUCacheSet {
        int assoc;
        std::list<uint32_t> lru_order;
        std::unordered_map<uint32_t, CacheLine> lines;

    public:
        LRUCacheSet(int a) : assoc(a) {}

        CacheLine* find(uint32_t tag) {
            auto it = lines.find(tag);
            if (it == lines.end()) return nullptr;
            lru_order.remove(tag);
            lru_order.push_front(tag);
            return &(it->second);
        }

        std::pair<CacheLine*, std::optional<CacheLine>> allocate(uint32_t tag) {
            std::optional<CacheLine> evicted = std::nullopt;
            if (lines.size() >= (size_t)assoc) {
                uint32_t victim_tag = lru_order.back();
                evicted = lines[victim_tag];
                lines.erase(victim_tag);
                lru_order.pop_back();
            }
            lru_order.push_front(tag);
            lines[tag] = {tag, DragonState::INVALID};
            return {&lines[tag], evicted};
        }
    };

    class DragonCache {
        std::vector<LRUCacheSet> sets;
        int set_bits, offset_bits, block_size;

    public:
        DragonCache(int size, int assoc, int bsize) : block_size(bsize) {
            int num_sets = size / (assoc * bsize);
            set_bits = __builtin_ctz(num_sets);
            offset_bits = __builtin_ctz(bsize);
            for (int i = 0; i < num_sets; i++) sets.emplace_back(assoc);
        }

        CacheLine* get_line(uint32_t addr) {
            uint32_t set_idx = (addr >> offset_bits) & (sets.size() - 1);
            uint32_t tag = addr >> (offset_bits + set_bits);
            return sets[set_idx].find(tag);
        }

        std::pair<CacheLine*, std::optional<CacheLine>> allocate(uint32_t addr) {
            uint32_t set_idx = (addr >> offset_bits) & (sets.size() - 1);
            uint32_t tag = addr >> (offset_bits + set_bits);
            return sets[set_idx].allocate(tag);
        }
    };

    class DragonProtocol : public CoherenceProtocol {
    private:
        std::vector<DragonCache> caches;
        int num_cores, block_size;
        GlobalStatistics& g_stats;
        std::vector<Statistics>& c_stats;

    public:
        DragonProtocol(int n, int cs, int as, int bs, GlobalStatistics& gs, std::vector<Statistics>& cs_ref) 
            : num_cores(n), block_size(bs), g_stats(gs), c_stats(cs_ref) {
            for (int i = 0; i < n; i++) caches.emplace_back(cs, as, bs);
        }

        ProtocolResponse handle_processor_request(int id, uint32_t addr, bool is_write) override {
            CacheLine* line = caches[id].get_line(addr);

            if (line && line->state != DragonState::INVALID) {
                // HIT (Valid Line Exists)
                
                if (is_write) {
                    // Write to Shared (Sc or Sm) -> Needs BusUpdate
                    if (line->state == DragonState::SHARED_CLEAN || line->state == DragonState::SHARED_MODIFIED) {
                        return { false, BusTransactionType::BUS_UPGR }; // Treat as miss/stall to broadcast update
                    }
                    
                    // Write to Private (E or M) -> Silent Hit
                    if (line->state == DragonState::EXCLUSIVE) {
                        line->state = DragonState::MODIFIED;
                    }
                }

                // Stats: HITS
                if (line->state == DragonState::SHARED_CLEAN || line->state == DragonState::SHARED_MODIFIED) {
                    c_stats[id].shared_accesses++;
                } else {
                    c_stats[id].private_accesses++;
                }

                return { true, BusTransactionType::NONE };
            }

            // MISS (Not in cache)
            // Dragon fetches block on Miss (even write miss) then updates it
            return { false, BusTransactionType::BUS_RD }; 
        }

        int handle_bus_transaction(int req_id, BusTransactionType type, uint32_t addr) override {
            bool shared_signal = false;

            // 1. SNOOPING LOOP
            for (int i = 0; i < num_cores; i++) {
                if (i == req_id) continue;
                CacheLine* snoop_line = caches[i].get_line(addr);

                if (snoop_line && snoop_line->state != DragonState::INVALID) {
                    shared_signal = true;

                    // CASE: BusUpdate (BUS_UPGR)
                    // Someone else is writing to this shared line.
                    if (type == BusTransactionType::BUS_UPGR) {
                        // Update local data (Simulator doesn't track data values, but logically it happens)
                        // If I was the Owner (Sm), I lose ownership to the new writer? 
                        // Standard Dragon: Only one Sm allowed. The writer becomes Sm (or stays Sm).
                        // Snooper becomes Sc.
                        
                        if (snoop_line->state == DragonState::SHARED_MODIFIED) {
                            snoop_line->state = DragonState::SHARED_CLEAN;
                        }
                        // If Sc, stay Sc.
                        
                        c_stats[i].invalidations_received++; // Using this field to track "Updates Received"
                        g_stats.total_invalidations++;       // Using this to track Total Updates
                    }
                    
                    // CASE: BusRead (BUS_RD)
                    // Someone missed and wants the block.
                    else if (type == BusTransactionType::BUS_RD) {
                        // If I have E or M, I assert shared.
                        if (snoop_line->state == DragonState::EXCLUSIVE) {
                            snoop_line->state = DragonState::SHARED_CLEAN;
                        } else if (snoop_line->state == DragonState::MODIFIED) {
                            snoop_line->state = DragonState::SHARED_MODIFIED; // I become Owner
                        }
                        // If Sc or Sm, I stay Sc or Sm.
                    }
                }
            }

            // 2. TIMING & STATE UPDATE (REQUESTER)
            int latency = 0;

            if (type == BusTransactionType::WRITEBACK) {
                latency = 100;
                g_stats.bus_writebacks++;
                g_stats.total_data_traffic_bytes += block_size;
            } 
            else if (type == BusTransactionType::BUS_UPGR) {
                // BusUpdate
                latency = 2; // "Sending a word... takes only 2 cycles"
                g_stats.bus_upgrades++; // Tracking Updates count
                g_stats.total_data_traffic_bytes += 4; // Word size traffic
                
                // Requester Logic:
                auto line = caches[req_id].get_line(addr);
                // If Shared Signal is still HIGH, stay Shared (Sm).
                // If Shared Signal is LOW (others evicted it?), upgrade to M.
                if (shared_signal) {
                    line->state = DragonState::SHARED_MODIFIED;
                } else {
                    line->state = DragonState::MODIFIED;
                }
            } 
            else {
                // BusRead
                bool cache_to_cache = shared_signal; 
                latency = cache_to_cache ? (1 + 2 * (block_size / 4)) : 101; // 17 vs 101

                if (cache_to_cache) g_stats.cache_to_cache_transfers++;
                g_stats.total_data_traffic_bytes += block_size;
                g_stats.bus_reads++;

                // Allocate
                auto [line, evicted] = caches[req_id].allocate(addr);

                // Handle Eviction Writeback
                // M and Sm are Dirty. E and Sc are Clean.
                if (evicted && (evicted->state == DragonState::MODIFIED || evicted->state == DragonState::SHARED_MODIFIED)) {
                    latency += 100;
                    g_stats.bus_writebacks++;
                    g_stats.total_data_traffic_bytes += block_size;
                }

                // New State
                if (shared_signal) {
                    line->state = DragonState::SHARED_CLEAN;
                } else {
                    line->state = DragonState::EXCLUSIVE;
                }
            }
            return latency;
        }

        void print_report(uint64_t total_cycles) override {
            std::cout << "===== Dragon Protocol Simulation Results =====" << std::endl;
            std::cout << "Overall Execution Cycles: " << total_cycles << std::endl;
            std::cout << "Total Data Traffic: " << g_stats.total_data_traffic_bytes << " bytes" << std::endl;
            std::cout << "Total Updates on Bus: " << g_stats.total_invalidations << std::endl;
            std::cout << "Bus Transactions: Rd=" << g_stats.bus_reads << " Update=" << g_stats.bus_upgrades 
                      << " WB=" << g_stats.bus_writebacks << " C2C=" << g_stats.cache_to_cache_transfers << std::endl;

            for (int i = 0; i < num_cores; ++i) {
                std::cout << "\n--- Core " << i << " ---" << std::endl;
                int ls = c_stats[i].load_count + c_stats[i].store_count;
                uint64_t idle = (total_cycles > (c_stats[i].compute_cycles + ls)) ? 
                                 total_cycles - c_stats[i].compute_cycles - ls : 0;
                
                std::cout << "Compute Cycles: " << c_stats[i].compute_cycles << std::endl;
                std::cout << "Load/Store Instr: " << ls << std::endl;
                std::cout << "Idle Cycles:    " << idle << std::endl;
                std::cout << "Hits: " << c_stats[i].cache_hits << " | Misses: " << c_stats[i].cache_misses << std::endl;
                
                double hit_rate = (ls > 0) ? 100.0 * c_stats[i].cache_hits / (c_stats[i].cache_hits + c_stats[i].cache_misses) : 0;
                std::cout << "Hit Rate: " << std::fixed << std::setprecision(2) << hit_rate << "%" << std::endl;

                int total_acc = c_stats[i].private_accesses + c_stats[i].shared_accesses;
                if (total_acc > 0) {
                    std::cout << "Access Dist: Private=" << std::fixed << std::setprecision(1) 
                              << (100.0 * c_stats[i].private_accesses / total_acc) << "% | Shared=" 
                              << (100.0 * c_stats[i].shared_accesses / total_acc) << "%" << std::endl;
                }
                std::cout << "Updates Received: " << c_stats[i].invalidations_received << std::endl;
            }
        }
    };
}
#endif