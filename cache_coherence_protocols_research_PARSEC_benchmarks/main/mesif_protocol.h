#ifndef MESIF_PROTOCOL_H
#define MESIF_PROTOCOL_H

#include "coherence_defs.h"
#include <vector>
#include <unordered_map>
#include <list>
#include <iostream>
#include <iomanip>
#include <optional>

namespace MESIF {

    enum class MESIFState { INVALID, SHARED, EXCLUSIVE, MODIFIED, FORWARD };

    struct CacheLine {
        uint32_t tag;
        MESIFState state = MESIFState::INVALID;
    };

    // ============================================================================
    // LRU CACHE SET (Identical logic)
    // ============================================================================
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
            lines[tag] = {tag, MESIFState::INVALID};
            return {&lines[tag], evicted};
        }
    };

    class MESIFCache {
        std::vector<LRUCacheSet> sets;
        int set_bits, offset_bits, block_size;

    public:
        MESIFCache(int size, int assoc, int bsize) : block_size(bsize) {
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

    // ============================================================================
    // MESIF PROTOCOL IMPLEMENTATION
    // ============================================================================
    class MESIFProtocol : public CoherenceProtocol {
    private:
        std::vector<MESIFCache> caches;
        int num_cores, block_size;
        GlobalStatistics& g_stats;
        std::vector<Statistics>& c_stats;

    public:
        MESIFProtocol(int n, int cs, int as, int bs, GlobalStatistics& gs, std::vector<Statistics>& cs_ref) 
            : num_cores(n), block_size(bs), g_stats(gs), c_stats(cs_ref) {
            for (int i = 0; i < n; i++) caches.emplace_back(cs, as, bs);
        }

        // ------------------------------------------------------------------------
        // PROCESSOR REQUEST
        // ------------------------------------------------------------------------
        ProtocolResponse handle_processor_request(int id, uint32_t addr, bool is_write) override {
            CacheLine* line = caches[id].get_line(addr);

            if (line && line->state != MESIFState::INVALID) {
                // HIT
                if (is_write) {
                    // Writes to Shared or Forward need Upgrade
                    if (line->state == MESIFState::SHARED || line->state == MESIFState::FORWARD) {
                        return { false, BusTransactionType::BUS_UPGR }; 
                    }
                    line->state = MESIFState::MODIFIED;
                }
                
                // Stats: F state counts as a Shared hit
                if (line->state == MESIFState::SHARED || line->state == MESIFState::FORWARD) {
                    c_stats[id].shared_accesses++;
                } else {
                    c_stats[id].private_accesses++;
                }

                return { true, BusTransactionType::NONE };
            }
            // MISS
            return { false, is_write ? BusTransactionType::BUS_RDX : BusTransactionType::BUS_RD };
        }

        // ------------------------------------------------------------------------
        // BUS TRANSACTION (The Core Optimization Logic)
        // ------------------------------------------------------------------------
        int handle_bus_transaction(int req_id, BusTransactionType type, uint32_t addr) override {
            bool shared_signal = false;
            bool supplier_found = false; // Does a specific cache (M, E, F) supply data?

            // 1. SNOOPING
            for (int i = 0; i < num_cores; i++) {
                if (i == req_id) continue;
                CacheLine* snoop_line = caches[i].get_line(addr);
                
                if (snoop_line && snoop_line->state != MESIFState::INVALID) {
                    shared_signal = true;
                    
                    // Identify Supplier: Only M, E, or F can supply data. 
                    // Standard S cannot supply (avoids bus contention).
                    if (snoop_line->state == MESIFState::MODIFIED || 
                        snoop_line->state == MESIFState::EXCLUSIVE || 
                        snoop_line->state == MESIFState::FORWARD) {
                        supplier_found = true;
                    }

                    // State Transitions
                    if (type == BusTransactionType::BUS_RDX || type == BusTransactionType::BUS_UPGR) {
                        // Invalidation
                        snoop_line->state = MESIFState::INVALID;
                        c_stats[i].invalidations_received++;
                        c_stats[req_id].invalidations_sent++;
                        g_stats.total_invalidations++;
                    } 
                    else if (type == BusTransactionType::BUS_RD) {
                        // Downgrade rules:
                        // M -> S (and flush to memory/requester)
                        // E -> S
                        // F -> S (Role migration: Requester becomes new F)
                        // S -> S
                        snoop_line->state = MESIFState::SHARED;
                    }
                }
            }

            // 2. TIMING & UPDATE
            int latency = 0;

            if (type == BusTransactionType::WRITEBACK) {
                latency = 100;
                g_stats.bus_writebacks++;
                g_stats.total_data_traffic_bytes += block_size;
            } 
            else if (type == BusTransactionType::BUS_UPGR) {
                latency = 0;
                g_stats.bus_upgrades++;
                caches[req_id].get_line(addr)->state = MESIFState::MODIFIED;
            } 
            else {
                // BUS_RD or BUS_RDX
                // MESIF OPTIMIZATION: If supplier_found (F/E/M), use C2C. Else Memory.
                bool cache_to_cache = supplier_found; 
                latency = cache_to_cache ? (1 + 2 * (block_size / 4)) : 101; // 17 vs 101

                if (cache_to_cache) g_stats.cache_to_cache_transfers++;
                g_stats.total_data_traffic_bytes += block_size;

                if (type == BusTransactionType::BUS_RD) g_stats.bus_reads++;
                else g_stats.bus_readx++;

                // Allocate
                auto [line, evicted] = caches[req_id].allocate(addr);
                
                // Implicit WB for Modified Eviction
                if (evicted && evicted->state == MESIFState::MODIFIED) {
                    latency += 100;
                    g_stats.bus_writebacks++;
                    g_stats.total_data_traffic_bytes += block_size;
                }

                // NEW STATE ASSIGNMENT
                if (type == BusTransactionType::BUS_RD) {
                    if (shared_signal) {
                        // If sharers exist, we become the FORWARDER
                        // (regardless of whether data came from C2C or Mem, usually C2C if S exists)
                        line->state = MESIFState::FORWARD;
                    } else {
                        // No sharers -> Exclusive
                        line->state = MESIFState::EXCLUSIVE;
                    }
                } else {
                    // Write Miss -> Modified
                    line->state = MESIFState::MODIFIED;
                }
            }
            return latency;
        }

        void print_report(uint64_t total_cycles) override {
            std::cout << "===== MESIF Protocol Simulation Results =====" << std::endl;
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