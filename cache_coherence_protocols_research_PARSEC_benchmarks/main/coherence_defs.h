#ifndef COHERENCE_DEFS_H
#define COHERENCE_DEFS_H

#include <cstdint>
#include <vector>
#include <string>

enum class BusTransactionType { BUS_RD, BUS_RDX, BUS_UPGR, WRITEBACK, NONE };

struct ProtocolResponse {
    bool is_hit;
    BusTransactionType bus_action;
};

struct Statistics {
    uint64_t compute_cycles = 0;
    int load_count = 0;
    int store_count = 0;
    int cache_hits = 0;
    int cache_misses = 0;
    int invalidations_sent = 0;
    int invalidations_received = 0;
    int private_accesses = 0; // Usage specific to protocol (e.g., Hit in M/E)
    int shared_accesses = 0;  // Usage specific to protocol (e.g., Hit in S)
};

struct GlobalStatistics {
    uint64_t total_data_traffic_bytes = 0;
    int total_invalidations = 0;
    int bus_reads = 0;
    int bus_readx = 0;
    int bus_upgrades = 0;
    int bus_writebacks = 0;
    int cache_to_cache_transfers = 0;
};

class CoherenceProtocol {
public:
    virtual ~CoherenceProtocol() = default;

    /**
     * Called by the core when it wants to access the cache.
     * Returns whether it was a hit and what bus action is required (if any).
     */
    virtual ProtocolResponse handle_processor_request(int core_id, uint32_t addr, bool is_write) = 0;

    /**
     * Called by the bus arbiter when a core wins the bus.
     * Protocol performs snooping on OTHER cores and state updates.
     * Returns the latency (cycles) this transaction occupies the bus.
     */
    virtual int handle_bus_transaction(int requester_id, BusTransactionType type, uint32_t addr) = 0;

    /**
     * Protocol implementation handles specific printing of metrics
     * (e.g., differentiating between Invalidations vs Updates).
     */
    virtual void print_report(uint64_t total_cycles) = 0;
};

#endif