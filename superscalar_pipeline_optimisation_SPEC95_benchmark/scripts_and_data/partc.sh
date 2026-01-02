#!/bin/bash
# Part C: L1 Cache Optimization - Using Correct default.cfg Format
echo "Choice,ICache_Sets,DCache_Sets,ICache_Config,DCache_Config,IPC" > partC_cache_optimization.csv

# Initialize tracking variables
best_ipc=0
best_choice=0
best_config=""
total_configs=7
successful_configs=0

echo "Starting Part C: L1 Cache Size Optimization"
echo "Using optimal Part A processor configuration:"
echo "W=4, IALU=3, IMULT=1, FPALU=1, FPMULT=1, RUU=32, LSQ=16, Area=59.28"
echo "Using default.cfg format: <name>:<nsets>:<bsize>:<assoc>:<repl>"
echo "Block size=64 bytes, Associativity=1 (direct-mapped), LRU replacement"
echo "=========================================="

# Define the 7 cache configuration choices
icache_sets=(1024 512 256 128 64 32 16)
dcache_sets=(16 32 64 128 256 512 1024)

# Fixed optimal processor parameters from Part A
W=4
IALU=3
IMULT=1
FPALU=1
FPMULT=1
RUU=32
LSQ=16

# Test each cache configuration choice
for choice in {1..7}; do
    icache=${icache_sets[$((choice-1))]}
    dcache=${dcache_sets[$((choice-1))]}
    
    # Use correct default.cfg format with cache names and LRU policy
    icache_config="il1:${icache}:64:1:l"
    dcache_config="dl1:${dcache}:64:1:l"
    
    echo -n "Testing Choice $choice: ICache=${icache} sets, DCache=${dcache} sets ... "
    
    # Build simulation command using correct format
    sim_cmd="timeout 300 ./../simplesim-3.0/sim-outorder -max:inst 80000000 \
        -fetch:ifqsize $W -decode:width $W -issue:width $W -commit:width $W \
        -issue:inorder false \
        -res:ialu $IALU -res:imult $IMULT -res:fpalu $FPALU -res:fpmult $FPMULT \
        -ruu:size $RUU -lsq:size $LSQ \
        -cache:il1 $icache_config \
        -cache:dl1 $dcache_config \
        go.ss 50 9 2stone9.in"
    
    # Run simulation
    eval "$sim_cmd" > temp_cache_${choice}.out 2>&1
    
    # Check results
    if [ $? -eq 0 ]; then
        ipc=$(grep "sim_IPC" temp_cache_${choice}.out | awk '{print $2}')
        
        if [ ! -z "$ipc" ] && [ "$ipc" != "0.0000" ]; then
            successful_configs=$((successful_configs + 1))
            echo "IPC=$ipc"
            
            # Save to CSV
            echo "$choice,$icache,$dcache,$icache_config,$dcache_config,$ipc" >> partC_cache_optimization.csv
            
            # Update best configuration
            if (( $(echo "$ipc > $best_ipc" | bc -l) )); then
                best_ipc=$ipc
                best_choice=$choice
                best_config="Choice $choice: ICache=${icache} sets, DCache=${dcache} sets, IPC=$ipc"
            fi
        else
            echo "FAILED (No IPC)"
        fi
    else
        echo "FAILED (Timeout/Error)"
    fi
    
    rm -f temp_cache_${choice}.out
done

# Results summary
echo "================================================"
echo "PART C: L1 CACHE OPTIMIZATION COMPLETE"
echo "================================================"
echo "Total cache configurations tested: $total_configs"
echo "Successful simulations: $successful_configs"
echo "Success rate: $(echo "scale=1; $successful_configs * 100 / $total_configs" | bc -l)%"
echo ""

if [ $successful_configs -gt 0 ]; then
    echo "🏆 BEST CACHE CONFIGURATION:"
    echo "$best_config"
    echo ""
    echo "📊 Performance comparison:"
    echo "Part A baseline (default cache): IPC = 0.9376"
    echo "Optimized cache configuration: IPC = $best_ipc"
    improvement=$(echo "scale=2; ($best_ipc - 0.9376) / 0.9376 * 100" | bc -l)
    echo "Cache optimization improvement: ${improvement}%"
    echo ""
    echo "📈 ALL CACHE CONFIGURATION RESULTS:"
    echo "Choice,ICache_Sets,DCache_Sets,ICache_Config,DCache_Config,IPC"
    tail -n +2 partC_cache_optimization.csv | sort -t',' -k6 -nr
    
    echo ""
    echo "Results saved to: partC_cache_optimization.csv"
else
    echo "❌ No successful simulations completed!"
fi

echo ""
echo "Part C (L1 Cache Optimization) complete!"

