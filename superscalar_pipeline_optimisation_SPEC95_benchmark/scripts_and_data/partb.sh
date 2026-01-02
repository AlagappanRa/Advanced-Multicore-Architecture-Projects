#!/bin/bash
# Part B: Efficiency Optimization - In-Order Configurations (64 configs)
echo "W,IALU,IMULT,FPALU,FPMULT,OOO,RUU,LSQ,Area,IPC,Efficiency" > partB_inorder_64.csv

# Initialize tracking variables
best_efficiency=0
best_efficiency_config=""
total_configs=0
successful_configs=0
skipped_configs=0

echo "Starting Part B: Efficiency Optimization - In-Order Configurations"
echo "FP optimization applied: FPALU=1, FPMULT=1 (zero FP usage)"
echo "Target: Area ≤ 60, Maximize IPC/Area Efficiency"
echo "Testing 64 in-order configurations..."
echo "=========================================="

# Parameter ranges for in-order efficiency optimization
pipeline_widths=(1 2 3 4)        # 4 options
int_alus=(1 2 3 4)              # 4 options  
int_mults=(1 2 3 4)             # 4 options (no restriction for in-order)
fp_alus=(1)                     # 1 option (optimized - zero FP usage)
fp_mults=(1)                    # 1 option (optimized - zero FP usage)

# In-order configurations only (no RUU/LSQ needed)
for W in "${pipeline_widths[@]}"; do
    for ialu in "${int_alus[@]}"; do
        for imult in "${int_mults[@]}"; do
            for fpalu in "${fp_alus[@]}"; do
                for fpmult in "${fp_mults[@]}"; do
                    total_configs=$((total_configs + 1))
                    
                    # Calculate in-order area
                    area=$(echo "$W * 1.8 + $ialu * 2 + $imult * 3 + $fpalu * 4 + $fpmult * 5" | bc -l)
                    
                    # Skip if area exceeds budget
                    if (( $(echo "$area > 60" | bc -l) )); then
                        echo "Skipped [$total_configs]: W=$W, IALU=$ialu, IMULT=$imult, inorder (Area=$area > 60)"
                        skipped_configs=$((skipped_configs + 1))
                        continue
                    fi
                    
                    echo -n "Testing [$total_configs]: W=$W, IALU=$ialu, IMULT=$imult, inorder, Area=$area ... "
                    
                    # Build in-order simulation command
                    sim_cmd="timeout 300 ./../simplesim-3.0/sim-outorder -max:inst 80000000 \
                        -fetch:ifqsize $W -decode:width $W -issue:width $W -commit:width $W \
                        -issue:inorder true \
                        -res:ialu $ialu -res:imult $imult -res:fpalu $fpalu -res:fpmult $fpmult \
                        go.ss 50 9 2stone9.in"
                    
                    # Run simulation
                    eval "$sim_cmd" > temp_inorder_${total_configs}.out 2>&1
                    
                    # Check if simulation completed successfully
                    if [ $? -eq 0 ]; then
                        # Extract IPC
                        ipc=$(grep "sim_IPC" temp_inorder_${total_configs}.out | awk '{print $2}')
                        
                        if [ ! -z "$ipc" ] && [ "$ipc" != "0.0000" ]; then
                            successful_configs=$((successful_configs + 1))
                            efficiency=$(echo "scale=6; $ipc / $area" | bc -l)
                            
                            echo "IPC=$ipc, Efficiency=$efficiency"
                            
                            # Save to CSV (RUU=0, LSQ=0, OOO=0 for in-order)
                            echo "$W,$ialu,$imult,$fpalu,$fpmult,0,0,0,$area,$ipc,$efficiency" >> partB_inorder_64.csv
                            
                            # Update best efficiency configuration
                            if (( $(echo "$efficiency > $best_efficiency" | bc -l) )); then
                                best_efficiency=$efficiency
                                best_efficiency_config="W=$W, IALU=$ialu, IMULT=$imult, FPALU=$fpalu, FPMULT=$fpmult, inorder, Area=$area, IPC=$ipc, Efficiency=$efficiency"
                            fi
                        else
                            echo "FAILED (No IPC)"
                        fi
                    else
                        echo "FAILED (Timeout/Error)"
                    fi
                    
                    # Clean up temporary file
                    rm -f temp_inorder_${total_configs}.out
                done
            done
        done
    done
done

# Print in-order efficiency results
echo "================================================"
echo "PART B: IN-ORDER EFFICIENCY ANALYSIS COMPLETE"
echo "================================================"
echo "Total configurations tested: $total_configs"
echo "Configurations skipped (Area > 60): $skipped_configs"
echo "Configurations within budget: $((total_configs - skipped_configs))"
echo "Successful simulations: $successful_configs"
echo "Success rate: $(echo "scale=1; $successful_configs * 100 / ($total_configs - skipped_configs)" | bc -l)%"
echo ""

if [ $successful_configs -gt 0 ]; then
    echo "🏆 BEST IN-ORDER EFFICIENCY CONFIGURATION:"
    echo "$best_efficiency_config"
    echo ""
    echo "🔝 TOP 10 IN-ORDER CONFIGURATIONS BY EFFICIENCY:"
    echo "W,IALU,IMULT,FPALU,FPMULT,OOO,RUU,LSQ,Area,IPC,Efficiency"
    tail -n +2 partB_inorder_64.csv | sort -t',' -k11 -nr | head -10
    
    echo ""
    echo "📊 IN-ORDER EFFICIENCY STATISTICS:"
    echo "Best efficiency: $best_efficiency"
    echo "Min area baseline efficiency: 0.0327 (from W=1,IALU=1,IMULT=1 test)"
    
    echo ""
    echo "Results saved to: partB_inorder_64.csv"
    echo ""
    echo "⚡ Next step: Combine with Part A out-of-order data for complete efficiency analysis"
else
    echo "❌ No successful simulations completed!"
fi

echo ""
echo "In-order efficiency analysis complete!"

