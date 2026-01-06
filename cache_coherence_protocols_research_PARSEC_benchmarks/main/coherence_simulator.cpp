#include "mesi_protocol.h"
#include "dragon_protocol.h"
#include "mesif_protocol.h"
#include "mesi_smart.h"
#include "moesi_protocol.h"

#include <iostream>
#include <string>
#include <fstream>
#include <queue>
#include <vector>
#include <memory>
#include <algorithm>
#include <cctype>
#include <iomanip>

// #define ENABLE_DEBUG 1
#ifndef READ_PRIORITY
#define READ_PRIORITY 0
#endif

void debug_log(uint64_t time, const std::string& msg) {
    #if ENABLE_DEBUG
    std::cout << "[Time " << std::setw(4) << time << "] " << msg << std::endl;
    #endif
}

class Simulator;

struct CoreState {
    bool blocked = false;
};

struct BusRequest {
    int core_id;
    BusTransactionType type;
    uint32_t addr;
    uint64_t arrival_time;
};

class Event {
public:
    uint64_t timestamp;
    int core_id;
    virtual ~Event() = default;
    virtual void execute(Simulator& sim) = 0;
    virtual int get_priority() const = 0; 
    virtual std::string get_name() const = 0;
};

struct EventComparator {
    bool operator()(const std::unique_ptr<Event>& a, const std::unique_ptr<Event>& b) const {
        if (a->timestamp != b->timestamp) return a->timestamp > b->timestamp;
        return a->get_priority() > b->get_priority();
    }
};

template<typename T, typename Container = std::vector<T>, typename Compare = std::less<typename Container::value_type>>
class DebuggableQueue : public std::priority_queue<T, Container, Compare> {
public:
    const Container& get_container() const { return this->c; }
};

class EventQueue {
    DebuggableQueue<std::unique_ptr<Event>, std::vector<std::unique_ptr<Event>>, EventComparator> events;
    uint64_t current_time = 0;
public:
    void schedule(std::unique_ptr<Event> event) { events.push(std::move(event)); }
    bool empty() const { return events.empty(); }
    uint64_t get_time() const { return current_time; }
    void process_next(Simulator& sim);

    void print_state() const {
        const auto& container = events.get_container();
        if (container.empty()) {
            std::cout << "    [Event Queue]: <Empty>" << std::endl;
            return;
        }
        std::cout << "    [Event Queue]: ";
        for (const auto& ev : container) {
            std::cout << "{" << ev->get_name() << " C" << ev->core_id << " @" << ev->timestamp << "} ";
        }
        std::cout << std::endl;
    }
};

class Simulator {
public:
    EventQueue event_queue;
    std::unique_ptr<CoherenceProtocol> protocol;
    
    std::vector<CoreState> core_states;
    std::vector<std::ifstream> traces;
    std::vector<bool> trace_exhausted;
    
    std::vector<BusRequest> bus_queue;
    bool bus_busy = false;
    
    std::vector<Statistics> stats;
    GlobalStatistics global_stats;
    int num_cores;

    Simulator(const std::string& proto, const std::string& benchmark, int csize, int assoc, int bsize, int cores)
        : num_cores(cores) {
        
        for (int i = 0; i < num_cores; ++i) {
            core_states.emplace_back();
            stats.emplace_back();
            trace_exhausted.push_back(false);
            
            std::string path = "../" + benchmark + "_four/" + benchmark + "_" + std::to_string(i) + ".data";
            traces.emplace_back(path);
            if (!traces.back().is_open()) {
                throw std::runtime_error("Failed to open trace: " + path);
            }
        }

        if (proto == "MESI") {
            protocol = std::make_unique<MESI::MESIProtocol>(num_cores, csize, assoc, bsize, global_stats, stats);
        } else if (proto == "DRAGON") {
            protocol = std::make_unique<Dragon::DragonProtocol>(num_cores, csize, assoc, bsize, global_stats, stats);
        } else if (proto == "MESIF") {
            protocol = std::make_unique<MESIF::MESIFProtocol>(num_cores, csize, assoc, bsize, global_stats, stats);
        }  else if (proto == "SMART") {
            protocol = std::make_unique<MESI_SMART::MESISmartProtocol>(num_cores, csize, assoc, bsize, global_stats, stats);
        } else if (proto == "MOESI") {
            protocol = std::make_unique<MOESI::MOESIProtocol>(num_cores, csize, assoc, bsize, global_stats, stats);
        }
        else {
            throw std::runtime_error("Unknown protocol: " + proto + "; \nAccepted: MESI DRAGON MESIF SMART MOESI");
        }
    }

    void run();
    
    void print_debug_state() {
        std::cout << "--- State at Cycle " << event_queue.get_time() << " ---" << std::endl;
        std::cout << "    [Bus Wait Q ]: ";
        if (bus_queue.empty()) std::cout << "<Empty>";
        for (const auto& req : bus_queue) {
            std::cout << "[C" << req.core_id << " Wait:" << req.arrival_time << "] ";
        }
        std::cout << (bus_busy ? " (BUS BUSY)" : " (BUS FREE)") << std::endl;
        event_queue.print_state();
        std::cout << std::endl;
    }
};

class BusReleaseEvent : public Event {
public:
    int get_priority() const override { return 0; }
    std::string get_name() const override { return "BusRel"; }
    void execute(Simulator& sim) override;
};

class BusArbitrationEvent : public Event {
public:
    int get_priority() const override { return 1; }
    std::string get_name() const override { return "BusArb"; }
    void execute(Simulator& sim) override;
};

class CoreUnblockEvent : public Event {
public:
    int get_priority() const override { return 2; }
    std::string get_name() const override { return "Unblock"; }
    void execute(Simulator& sim) override;
};

class FetchEvent : public Event {
public:
    int get_priority() const override { return 3; }
    std::string get_name() const override { return "Fetch"; }
    void execute(Simulator& sim) override;
};

void BusReleaseEvent::execute(Simulator& sim) {
    debug_log(timestamp, "Bus Released");
    sim.bus_busy = false;
    
    // Immediate Handoff: If anyone is waiting, schedule Arbitration NOW.
    if (!sim.bus_queue.empty()) {
        auto arb = std::make_unique<BusArbitrationEvent>();
        arb->timestamp = timestamp;
        arb->core_id = -1;
        sim.event_queue.schedule(std::move(arb));
    }
}

void BusArbitrationEvent::execute(Simulator& sim) {
    if (sim.bus_busy || sim.bus_queue.empty()) return;

    auto it = std::min_element(sim.bus_queue.begin(), sim.bus_queue.end(),
        [](const BusRequest& a, const BusRequest& b) {
            if constexpr (READ_PRIORITY) {
                bool a_is_read = (a.type == BusTransactionType::BUS_RD);
                bool b_is_read = (b.type == BusTransactionType::BUS_RD);
                if (a_is_read != b_is_read) return a_is_read;
            }
            // Fallback to FCFS
            if (a.arrival_time != b.arrival_time) return a.arrival_time < b.arrival_time;
            return a.core_id < b.core_id;
        });
    
    BusRequest req = *it;
    sim.bus_queue.erase(it);
    sim.bus_busy = true;

    debug_log(timestamp, "Bus Granted to Core " + std::to_string(req.core_id) + " for Addr 0x" + std::to_string(req.addr));

    int latency = sim.protocol->handle_bus_transaction(req.core_id, req.type, req.addr);

    debug_log(timestamp, "Transaction Latency: " + std::to_string(latency) + " cycles. Ends at " + std::to_string(timestamp + latency));

    // 1. Schedule Release (P0)
    auto release = std::make_unique<BusReleaseEvent>();
    release->timestamp = timestamp + latency;
    release->core_id = -1;
    sim.event_queue.schedule(std::move(release));
    
    // 2. Schedule Core Unblock (P2)
    if (req.type != BusTransactionType::WRITEBACK) {
        auto unblock = std::make_unique<CoreUnblockEvent>();
        unblock->timestamp = timestamp + latency;
        unblock->core_id = req.core_id;
        sim.event_queue.schedule(std::move(unblock));
    }
}

void CoreUnblockEvent::execute(Simulator& sim) {
    debug_log(timestamp, "Core " + std::to_string(core_id) + " Unblocked");
    sim.core_states[core_id].blocked = false;
    
    auto fetch = std::make_unique<FetchEvent>();
    fetch->timestamp = timestamp;
    fetch->core_id = core_id;
    sim.event_queue.schedule(std::move(fetch));
}

void FetchEvent::execute(Simulator& sim) {
    if (sim.core_states[core_id].blocked) return;

    int label; uint32_t val;
    if (!(sim.traces[core_id] >> label >> std::hex >> val)) {
        sim.trace_exhausted[core_id] = true;
        return;
    }

    if (label == 2) { // Compute
        sim.stats[core_id].compute_cycles += val;
        auto next = std::make_unique<FetchEvent>();
        next->timestamp = timestamp + val;
        next->core_id = core_id;
        sim.event_queue.schedule(std::move(next));
        debug_log(timestamp, "Core " + std::to_string(core_id) + " compute for " + std::to_string(val) + " cycles");
    } else { // Memory Access
        bool is_write = (label == 1);
        if (is_write) sim.stats[core_id].store_count++; else sim.stats[core_id].load_count++;
        
        auto resp = sim.protocol->handle_processor_request(core_id, val, is_write);

        if (resp.is_hit && resp.bus_action == BusTransactionType::NONE) {
            sim.stats[core_id].cache_hits++;
            auto next = std::make_unique<FetchEvent>();
            next->timestamp = timestamp + 1;
            next->core_id = core_id;
            sim.event_queue.schedule(std::move(next));
            debug_log(timestamp, "Core " + std::to_string(core_id) + " HIT on 0x" + std::to_string(val));
        } else {
            debug_log(timestamp, "Core " + std::to_string(core_id) + " MISS on 0x" + std::to_string(val) + " -> Queueing Bus Req");
            sim.stats[core_id].cache_misses++;
            sim.core_states[core_id].blocked = true;
            
            sim.bus_queue.push_back({core_id, resp.bus_action, val, timestamp});
            
            if (!sim.bus_busy) {
                auto arb = std::make_unique<BusArbitrationEvent>();
                arb->timestamp = timestamp;
                arb->core_id = -1; 
                sim.event_queue.schedule(std::move(arb));
            }
        }
    }
}

void EventQueue::process_next(Simulator& sim) {
    if (events.empty()) return;
    auto event = std::move(const_cast<std::unique_ptr<Event>&>(events.top()));
    events.pop();
    current_time = event->timestamp;
    event->execute(sim);
}

void Simulator::run() {
    for (int i = 0; i < num_cores; ++i) {
        auto ev = std::make_unique<FetchEvent>();
        ev->timestamp = 0;
        ev->core_id = i;
        event_queue.schedule(std::move(ev));
    }

    while (!event_queue.empty()) {
        // Uncomment to see the detailed timeline
        //print_debug_state(); 
        
        event_queue.process_next(*this);
        
        bool done = std::all_of(trace_exhausted.begin(), trace_exhausted.end(), [](bool b){return b;});
        bool idle = std::all_of(core_states.begin(), core_states.end(), [](const CoreState& s){return !s.blocked;});
        if (done && idle && !bus_busy && bus_queue.empty()) break;
    }
    
    protocol->print_report(event_queue.get_time());
}

int main(int argc, char* argv[]) {
    if (argc != 6) {
        std::cerr << "Usage: ./coherence <PROTOCOL> <BENCHMARK> <CACHE_SIZE> <ASSOC> <BLOCK_SIZE>\n";
        return 1;
    }
    
    try {
        std::string proto = argv[1];
        std::transform(proto.begin(), proto.end(), proto.begin(), ::toupper);
        Simulator sim(proto, argv[2], std::stoi(argv[3]), std::stoi(argv[4]), std::stoi(argv[5]), 4);
        sim.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}