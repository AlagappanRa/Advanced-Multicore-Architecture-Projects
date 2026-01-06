# Advanced Multi-Core Architecture Projects

[![Language](https://img.shields.io/badge/Language-C%2B%2B17-blue)]()
[![Domain](https://img.shields.io/badge/Domain-Computer%20Architecture%20%7C%20Systems-orange)]()
[![Tools](https://img.shields.io/badge/Tools-SimpleScalar%20%7C%20Discrete%20Event%20Sim-green)]()

> **Context:** Advanced systems coursework (CS4223 Multi-Core Architectures) at the National University of Singapore (NUS).

## Overview
This repository contains two rigorous systems engineering projects focusing on the hardware-software interface. 
1.  **Cycle-Accurate Cache Coherence Simulator:** A discrete-event simulator written in C++ to benchmark MESI, Dragon, and MOESI protocols.
2.  **Superscalar Pipeline Optimization:** A design space exploration study optimizing processor microarchitecture for the SPEC95 benchmark suite under strict silicon area constraints.

---

## Project 1: Cycle-Accurate Cache Coherence Simulator

### Objective
To architect a trace-driven, discrete-event simulator capable of modeling memory consistency and bus arbitration in a multi-core processor environment. The goal was to analyze the trade-offs between **Invalidation-based (MESI)** and **Update-based (Dragon)** protocols under varying workload characteristics.

### System Architecture
The simulator models a 4-core system with the following specifications:
*   **Core Model:** Single-issue, blocking cache logic.
*   **Memory Hierarchy:** Private L1 Data Caches (Write-Back/Write-Allocate) backed by Main Memory.
*   **Interconnect:** Shared Bus with FIFO arbitration and atomic transactions.
*   **Simulation Engine:** Custom C++ event queue handling `InstructionFetch`, `MemoryAccess`, `BusTransaction`, and `coherence` events.

### Key Implementations
I implemented three distinct protocols from scratch:
1.  **MESI (Modified, Exclusive, Shared, Invalid):** Standard invalidation protocol. Silent E→M transitions implemented to reduce bus traffic.
2.  **Dragon (Update-based):** A 4-state protocol (M, E, Sc, Sm) that broadcasts word-level updates rather than invalidating cache lines.
3.  **MOESI (Optimization):** An extension of MESI introducing the **Owned (O)** state to allow dirty sharing and reduce main memory writebacks.

### Performance Analysis
We benchmarked the protocols using the **PARSEC Suite** (*Blackscholes, Bodytrack, Fluidanimate*).

![Coherence Results](./assets/coherence_results.png)
*(Figure 1: Comparative analysis of execution cycles across protocols. Note the massive speedup with Dragon.)*

**Key Insight: The Update Paradox**
Contrary to the intuition that invalidation protocols are superior for bandwidth conservation, the **Dragon protocol** achieved a **40x speedup** over MESI for the *Blackscholes* benchmark.
*   **Why:** Although Dragon generated **35x more bus traffic** (due to continuous updates), it eliminated the expensive 100-cycle penalties associated with cache misses in MESI.
*   **Conclusion:** In systems with sufficient bus bandwidth, update-based protocols can drastically outperform invalidation protocols for write-sharing workloads.

[**Read the Full Technical Report**](./cache_coherence_protocols_research_PARSEC_benchmarks/Cache-Coherence-Simulator-Report-CS4223.pdf)

---

## Project 2: Superscalar Pipeline Optimization

### Objective
To design the optimal superscalar microarchitecture for the **SPEC95 "Go" benchmark** (099.go), maximizing Instructions Per Cycle (IPC) while strictly adhering to a **Chip Area Constraint of 60 units**.

### Methodology
*   **Tool:** SimpleScalar (sim-outorder).
*   **Constraints:** Area cost modeling based on register units (RUU), Load/Store Queues (LSQ), and Functional Units (ALU/FPU).
*   **Strategy:** Automated design space exploration to prune suboptimal configurations.

### Optimization Strategy
1.  **Workload Profiling:** Analysis of instruction distribution revealed that the "Go" benchmark had **0% Floating Point utilization**.
2.  **Area Pruning:** I removed all Floating Point Units (FPU) and reduced Integer Multipliers to the minimum (1 unit), reclaiming 20+ area units.
3.  **Resource Reallocation:** Reallocated the saved area budget to expand the **Instruction Window (RUU)** and **Load/Store Queue (LSQ)**, which were identified as the primary bottlenecks for IPC.

![Pipeline Optimization](./assets/pipeline_results.png)
*(Figure 2: Design space exploration showing the Pareto frontier of IPC vs. Area.)*

### Key Findings
*   **In-Order vs. Out-of-Order:** While Out-of-Order execution provided the highest raw performance (IPC = 0.93), a minimal **In-Order architecture** proved to be the most efficient design in terms of **Performance-per-Watt**, doubling the efficiency metric of the complex OOO designs.

[**Read the Optimization Report**](./Processor-Pipeline-Optimization/Report_Pipeline_Optimization.pdf)

---

## Tech Stack
*   **Languages:** C++17, Python (Analysis scripts), Bash
*   **Tools:** SimpleScalar, GCC, Make
*   **Concepts:** Memory Consistency Models, Race Conditions, Discrete-Event Simulation, Microarchitectural Tuning.

---

## Citation
If you find these implementations useful for reference, please cite:

```bibtex
@misc{ramanathan2025multicore,
  author = {Alagappan Ramanathan},
  title = {Advanced Multi-Core Architecture and Cache Coherence Simulation},
  year = {2025},
  publisher = {GitHub},
  journal = {CS4223 Coursework, National University of Singapore}
}