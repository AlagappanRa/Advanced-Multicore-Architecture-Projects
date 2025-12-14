#!/bin/bash
# Part A: IPC Maximization - Out-of-Order Only (288 configurations)
echo "W,IALU,IMULT,FPALU,FPMULT,OOO,RUU,LSQ,Area,IPC,Efficiency" > partA_outorder_288.csv

# Initialize tracking variables
best_ipc=0
best_ipc_config=""
total_configs=0
successful_configs=0
skipped_configs=0

echo "Starting Part A: IPC Maximization (Out-of-Order Only)"
echo "Based on max in-order test: IPC=0.6408 < baseline=0.9118"
echo "All in-order configurations eliminated from search space"
echo "FP optimization applied: FPALU=1, FPMULT=1 (zero FP usage)"
echo "Target: Area ≤ 60, Maximize IPC"
echo "Testing 288 out-of-order configurations..."
echo "=========================================="

# Optimized parameter ranges based on profiling analysis
pipeline_widths=(1 2 3 4)        # 4 options
int_alus=(1 2 3 4)              # 4 options  
int_mults=(1 2)                 # 2 options (reduced from 4 due to low usage)
fp_alus=(1)                     # 1 option (optimized - zero FP usage)
fp_mults=(1)                    # 1 option (optimized - zero FP usage)
ruu_sizes=(8 16 32)             # 3 options
lsq_sizes=(4 8 16)              # 3 options

# Out-of-order configurations only
for W in "${pipeline_widths[@]}"; do
    for ialu in "${int_alus[@]}"; do
        for imult in "${int_mults[@]}"; do
            for fpalu in "${fp_alus[@]}"; do
                for fpmult in "${fp_mults[@]}"; do
                    for ruu in "${ruu_sizes[@]}"; do
                        for lsq in "${lsq_sizes[@]}"; do
                            total_configs=$((total_configs + 1))
                            
                            # Calculate out-of-order area
                            area=$(echo "($W * 1.8 + $ialu * 2 + $imult * 3 + $fpalu * 4 + $fpmult * 5) * 1.4 + $ruu * 0.5 + $lsq * 0.5" | bc -l)
                            
                            # Skip if area exceeds budget
                            if (( $(echo "$area > 60" | bc -l) )); then
                                echo "Skipped [$total_configs]: W=$W, IALU=$ialu, IMULT=$imult, RUU=$ruu, LSQ=$lsq (Area=$area > 60)"
                                skipped_configs=$((skipped_configs + 1))
                                continue
                            fi
                            
                            echo -n "Testing [$total_configs]: W=$W, IALU=$ialu, IMULT=$imult, RUU=$ruu, LSQ=$lsq, Area=$area ... "
                            
                            # Build out-of-order simulation command
                            sim_cmd="timeout 300 ./../simplesim-3.0/sim-outorder -max:inst 80000000 \
                                -fetch:ifqsize $W -decode:width $W -issue:width $W -commit:width $W \
                                -issue:inorder false \
                                -res:ialu $ialu -res:imult $imult -res:fpalu $fpalu -res:fpmult $fpmult \
                                -ruu:size $ruu -lsq:size $lsq \
                                go.ss 50 9 2stone9.in"
                            
                            # Run simulation
                            eval "$sim_cmd" > temp_${total_configs}.out 2>&1
                            
                            # Check if simulation completed successfully
                            if [ $? -eq 0 ]; then
                                # Extract IPC
                                ipc=$(grep "sim_IPC" temp_${total_configs}.out | awk '{print $2}')
                                
                                if [ ! -z "$ipc" ] && [ "$ipc" != "0.0000" ]; then
                                    successful_configs=$((successful_configs + 1))
                                    efficiency=$(echo "scale=6; $ipc / $area" | bc -l)
                                    
                                    echo "IPC=$ipc, Efficiency=$efficiency"
                                    
                                    # Save to CSV
                                    echo "$W,$ialu,$imult,$fpalu,$fpmult,1,$ruu,$lsq,$area,$ipc,$efficiency" >> partA_outorder_288.csv
                                    
                                    # Update best IPC configuration
                                    if (( $(echo "$ipc > $best_ipc" | bc -l) )); then
                                        best_ipc=$ipc
                                        best_ipc_config="W=$W, IALU=$ialu, IMULT=$imult, FPALU=$fpalu, FPMULT=$fpmult, RUU=$ruu, LSQ=$lsq, Area=$area, IPC=$ipc, Efficiency=$efficiency"
                                    fi
                                else
                                    echo "FAILED (No IPC)"
                                fi
                            else
                                echo "FAILED (Timeout/Error)"
                            fi
                            
                            # Clean up temporary file
                            rm -f temp_${total_configs}.out
                        done
                    done
                done
            done
        done
    done
done

# Print Part A optimization results
echo "================================================"
echo "PART A: IPC MAXIMIZATION COMPLETE"
echo "================================================"
echo "Total configurations tested: $total_configs"
echo "Configurations skipped (Area > 60): $skipped_configs"
echo "Configurations within budget: $((total_configs - skipped_configs))"
echo "Successful simulations: $successful_configs"
echo "Success rate: $(echo "scale=1; $successful_configs * 100 / ($total_configs - skipped_configs)" | bc -l)%"
echo ""

if [ $successful_configs -gt 0 ]; then
    echo "🏆 BEST IPC CONFIGURATION (Part A):"
    echo "$best_ipc_config"
    echo ""
    echo "📊 Performance comparison:"
    echo "Baseline (default): IPC = 0.9118"
    echo "Best out-of-order: IPC = $best_ipc"
    improvement=$(echo "scale=2; ($best_ipc - 0.9118) / 0.9118 * 100" | bc -l)
    echo "IPC improvement: ${improvement}%"
    echo ""
    echo "🔝 TOP 10 CONFIGURATIONS BY IPC:"
    echo "W,IALU,IMULT,FPALU,FPMULT,OOO,RUU,LSQ,Area,IPC,Efficiency"
    tail -n +2 partA_outorder_288.csv | sort -t',' -k10 -nr | head -10
    
    echo ""
    echo "Results saved to: partA_outorder_288.csv"
else
    echo "❌ No successful simulations completed!"
fi

echo ""
echo "Part A (IPC Maximization) complete!"
