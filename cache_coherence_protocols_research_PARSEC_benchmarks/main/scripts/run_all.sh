#!/bin/bash

# Define the dimensions
PROTOCOLS=("MESI" "DRAGON" "MOESI" "MESIF" "SMART")
BENCHMARKS=("blackscholes" "bodytrack" "fluidanimate")

# Configs: "Size Assoc Block"
# 1. Tiny (High Capacity Misses)
# 2. Default (Baseline)
# 3. Large (Low Misses)
# 4. Large Block (Test False Sharing)
CONFIGS=(
    "1024 1 16"
    "4096 2 32"
    "8192 4 32"
    "8192 4 64"
)

# Output File
CSV_FILE="results_comprehensive.csv"
echo "Bus_Priority,Protocol,Benchmark,CacheSize,Assoc,BlockSize,Cycles,Traffic,Invalidations,Writebacks" > $CSV_FILE

# Function to run tests
run_suite() {
    PRIORITY=$1
    echo "--- Running Suite with Bus Read Priority = $PRIORITY ---"
    
    for proto in "${PROTOCOLS[@]}"; do
        for bench in "${BENCHMARKS[@]}"; do
            for conf in "${CONFIGS[@]}"; do
                # Split config string into variables
                read -r SIZE ASSOC BLOCK <<< "$conf"
                
                # Run Simulator
                OUTPUT=$(./coherence $proto $bench $SIZE $ASSOC $BLOCK)
                
                # Parse Output
                CYCLES=$(echo "$OUTPUT" | grep "Overall Execution Cycles" | awk '{print $4}')
                TRAFFIC=$(echo "$OUTPUT" | grep "Total Data Traffic" | awk '{print $4}')
                # Note: 'Total Invalidations' or 'Total Updates' depending on protocol. 
                # Ensure your print statements in Dragon/MESI align, or grep both.
                INVAL=$(echo "$OUTPUT" | grep -E "Total Invalidations|Total Updates on Bus" | awk '{print $NF}')
                
                # Extract Writebacks (handling the "WB=" format)
                WB=$(echo "$OUTPUT" | grep "Bus Transactions" | grep -o "WB=[0-9]*" | cut -d= -f2)
                
                # Append to CSV
                echo "$PRIORITY,$proto,$bench,$SIZE,$ASSOC,$BLOCK,$CYCLES,$TRAFFIC,$INVAL,$WB" >> $CSV_FILE
            done
        done
        echo "Completed $proto..."
    done
}

# 1. Compile and Run Standard FCFS
g++ -O3 ../coherence_simulator.cpp -o ../coherence
run_suite 0

# 2. Compile and Run Read Priority Optimization
g++ -O3 -DREAD_PRIORITY ../coherence_simulator.cpp -o ../coherence
run_suite 1

echo "All experiments finished. Data saved to $CSV_FILE"