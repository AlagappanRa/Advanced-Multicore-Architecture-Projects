#ifndef MESI_PROTOCOL_H
#define MESI_PROTOCOL_H

#include "coherence_defs.h"
#include <vector>
#include <unordered_map>
#include <list>
#include <iostream>
#include <iomanip>
#include <optional>

namespace MESI {

    enum class MESIState { INVALID, SHARED, EXCLUSIVE, MODIFIED };

    struct CacheLine {
        uint32_t tag;
        MESIState state = MESIState::INVALID;
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

        // Returns {pointer_to_line, optional_evicted_line_copy}
        std::pair<CacheLine*, std::optional<CacheLine>> allocate(uint32_t tag) {
            std::optional<CacheLine> evicted = std::nullopt;
            if (lines.size() >= (size_t)assoc) {
                uint32_t victim_tag = lru_order.back();
                evicted = lines[victim_tag];
                lines.erase(victim_tag);
                lru_order.pop_back();
            }
            lru_order.push_front(tag);
            lines[tag] = {tag, MESIState::INVALID};
            return {&lines[tag], evicted};
        }
    };

    class MESICache {
        std::vector<LRUCacheSet> sets;
        int set_bits, offset_bits, block_size;

    public:
        MESICache(int size, int assoc, int bsize) : block_size(bsize) {
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

    class MESIProtocol : public CoherenceProtocol {
    private:
        std::vector<MESICache> caches;
        int num_cores, block_size;
        GlobalStatistics& g_stats;
        std::vector<Statistics>& c_stats;

    public:
        MESIProtocol(int n, int cs, int as, int bs, GlobalStatistics& gs, std::vector<Statistics>& cs_ref) 
            : num_cores(n), block_size(bs), g_stats(gs), c_stats(cs_ref) {
            for (int i = 0; i < n; i++) caches.emplace_back(cs, as, bs);
        }

        ProtocolResponse handle_processor_request(int id, uint32_t addr, bool is_write) override {
            CacheLine* line = caches[id].get_line(addr);

            if (line && line->state != MESIState::INVALID) {
                // HIT
                if (is_write) {
                    if (line->state == MESIState::SHARED) {
                        return { false, BusTransactionType::BUS_UPGR }; // Coherence Miss (Upgrade)
                    }
                    line->state = MESIState::MODIFIED;
                }
                
                // Req 8: Hit Distribution
                if (line->state == MESIState::SHARED) c_stats[id].shared_accesses++;
                else c_stats[id].private_accesses++; // M or E

                return { true, BusTransactionType::NONE };
            }
            
            // MISS
            return { false, is_write ? BusTransactionType::BUS_RDX : BusTransactionType::BUS_RD };
        }

        int handle_bus_transaction(int req_id, BusTransactionType type, uint32_t addr) override {
            bool shared_signal = false;

            // 1. SNOOPING
            for (int i = 0; i < num_cores; i++) {
                if (i == req_id) continue;
                CacheLine* snoop_line = caches[i].get_line(addr);
                
                if (snoop_line && snoop_line->state != MESIState::INVALID) {
                    shared_signal = true;
                    // Transitions for snooped caches => Invalidations
                    if (type == BusTransactionType::BUS_RDX || type == BusTransactionType::BUS_UPGR) {
                        snoop_line->state = MESIState::INVALID;
                        c_stats[i].invalidations_received++;
                        c_stats[req_id].invalidations_sent++;
                        g_stats.total_invalidations++;
                    } else if (type == BusTransactionType::BUS_RD) {
                        if (snoop_line->state == MESIState::MODIFIED || snoop_line->state == MESIState::EXCLUSIVE) {
                            snoop_line->state = MESIState::SHARED;
                        }
                    }
                }
            }

            // 2. TIMING & STATE UPDATE
            int latency = 0;

            if (type == BusTransactionType::WRITEBACK) {
                latency = 100;
                g_stats.bus_writebacks++;
                g_stats.total_data_traffic_bytes += block_size;
            } else if (type == BusTransactionType::BUS_UPGR) {
                latency = 0; // BusUpgr is usually just an invalidation signal (very fast/addr only)
                g_stats.bus_upgrades++;
                caches[req_id].get_line(addr)->state = MESIState::MODIFIED;
            } else {
                // BUS_RD or BUS_RDX
                bool cache_to_cache = shared_signal; 
                latency = cache_to_cache ? (1 + 2 * (block_size / 4)) : 101;

                if (cache_to_cache) g_stats.cache_to_cache_transfers++;
                g_stats.total_data_traffic_bytes += block_size;

                if (type == BusTransactionType::BUS_RD) g_stats.bus_reads++;
                else g_stats.bus_readx++;

                // Allocate and set state
                auto [line, evicted] = caches[req_id].allocate(addr);
                
                // If eviction was Modified, add Writeback cost implicitly here for simplicity
                if (evicted && evicted->state == MESIState::MODIFIED) {
                    latency += 100;
                    g_stats.bus_writebacks++;
                    g_stats.total_data_traffic_bytes += block_size;
                }

                line->state = (type == BusTransactionType::BUS_RD && shared_signal) ? MESIState::SHARED 
                             : (type == BusTransactionType::BUS_RD) ? MESIState::EXCLUSIVE 
                             : MESIState::MODIFIED;
            }
            return latency;
        }

        void print_report(uint64_t total_cycles) override {
            std::cout << "===== MESI Protocol Simulation Results =====" << std::endl;
            std::cout << "Overall Execution Cycles: " << total_cycles << std::endl;
            std::cout << "Total Data Traffic: " << g_stats.total_data_traffic_bytes << " bytes" << std::endl;
            std::cout << "Total Invalidations: " << g_stats.total_invalidations << std::endl;
            std::cout << "Bus Transactions: Rd=" << g_stats.bus_reads << " RdX=" << g_stats.bus_readx 
                      << " Upgr=" << g_stats.bus_upgrades << " WB=" << g_stats.bus_writebacks 
                      << " C2C=" << g_stats.cache_to_cache_transfers << std::endl;

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
                std::cout << "Invalidations RX: " << c_stats[i].invalidations_received << std::endl;
            }
        }
    };
}
#endif