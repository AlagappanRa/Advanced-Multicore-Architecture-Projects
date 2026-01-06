#!/bin/bash

PROTOCOLS=("MESI" "DRAGON" "MOESI" "MESIF" "SMART")
BENCHMARKS=("blackscholes" "bodytrack" "fluidanimate")
CONFIG="8192 4 32" # Cache size, Assoc, Block size

echo "Bus_Read_Priority,Protocol,Benchmark,Cycles,Traffic,Invalidations,Writebacks" > results.csv
g++ -O3 ../coherence_simulator.cpp -o ../coherence && echo "Compiled coherence with simulator with FCFS Bus"

for proto in "${PROTOCOLS[@]}"; do
    for bench in "${BENCHMARKS[@]}"; do
        # Run sim and capture output
        OUTPUT=$(../coherence $proto $bench $CONFIG)
        
        # Parse using grep/awk (Adjust based on your exact print_report output)
        CYCLES=$(echo "$OUTPUT" | grep "Overall Execution Cycles" | awk '{print $4}')
        TRAFFIC=$(echo "$OUTPUT" | grep "Total Data Traffic" | awk '{print $4}')
        INVAL=$(echo "$OUTPUT" | grep "Total Invalidations" | awk '{print $3}')
        # Note: You might need to update your print_report to make parsing Writebacks easier
        # Or look for "WB=" in the Bus Transactions line
        WB=$(echo "$OUTPUT" | grep "Bus Transactions" | grep -o "WB=[0-9]*" | cut -d= -f2)
        
        echo "0,$proto,$bench,$CYCLES,$TRAFFIC,$INVAL,$WB" >> results.csv
        echo "Finished $proto on $bench"
    done
done

g++ -O3 -DREAD_PRIORITY=1 ../coherence_simulator.cpp -o ../coherence && echo "Compiled coherence with Bus Rd > Writes on Bus"

for proto in "${PROTOCOLS[@]}"; do
    for bench in "${BENCHMARKS[@]}"; do
        # Run sim and capture output
        OUTPUT=$(../coherence $proto $bench $CONFIG)
        
        # Parse using grep/awk (Adjust based on your exact print_report output)
        CYCLES=$(echo "$OUTPUT" | grep "Overall Execution Cycles" | awk '{print $4}')
        TRAFFIC=$(echo "$OUTPUT" | grep "Total Data Traffic" | awk '{print $4}')
        INVAL=$(echo "$OUTPUT" | grep "Total Invalidations" | awk '{print $3}')
        # Note: You might need to update your print_report to make parsing Writebacks easier
        # Or look for "WB=" in the Bus Transactions line
        WB=$(echo "$OUTPUT" | grep "Bus Transactions" | grep -o "WB=[0-9]*" | cut -d= -f2)
        
        echo "1,$proto,$bench,$CYCLES,$TRAFFIC,$INVAL,$WB" >> results.csv
        echo "Finished $proto on $bench"
    done
done