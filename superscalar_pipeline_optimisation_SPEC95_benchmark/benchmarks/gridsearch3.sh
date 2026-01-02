#!/bin/bash

echo "Starting stubbed loop test for 288 configurations..."
echo "=========================================="

# Initialize counter
total_configs=0

# Define parameter ranges
pipeline_widths=(1 2 3 4)
int_alus=(1 2 3 4)
int_mults=(1 2)
fp_alus=(1)        # Fixed at 1 (FP optimization)
fp_mults=(1)       # Fixed at 1 (FP optimization)
execution_modes=("inorder" "outorder")
ruu_sizes=(8 16 32)
lsq_sizes=(4 8 16)   # Full range for all RUU sizes

for W in "${pipeline_widths[@]}"; do
    for ialu in "${int_alus[@]}"; do
        for imult in "${int_mults[@]}"; do
            for fpalu in "${fp_alus[@]}"; do
                for fpmult in "${fp_mults[@]}"; do
                    for exec_mode in "${execution_modes[@]}"; do
                        for ruu in "${ruu_sizes[@]}"; do
                            for lsq in "${lsq_sizes[@]}"; do
                                total_configs=$((total_configs + 1))
                                echo "Loop iteration: $total_configs --> W=$W, IALU=$ialu, IMULT=$imult, FPALU=$fpalu, FPMULT=$fpmult, ExecMode=$exec_mode, RUU=$ruu, LSQ=$lsq"
                            done
                        done
                    done
                done
            done
        done
    done
done

echo "=========================================="
echo "Total loop iterations: $total_configs"

if [ $total_configs -eq 576 ]; then
    echo "✅ SUCCESS: Loop ran exactly 576 times as expected!"
    exit 0
else
    echo "❌ ERROR: Expected 576 iterations, but got $total_configs"
    exit 1
fi
