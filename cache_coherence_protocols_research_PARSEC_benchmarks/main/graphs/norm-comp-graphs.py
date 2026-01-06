import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np

# 1. Load the Comprehensive Data
df = pd.read_csv('../results_comprehensive.csv')

# Set visual style
sns.set_theme(style="whitegrid")
plt.rcParams.update({'font.size': 12})

def save_fig(name):
    plt.tight_layout()
    plt.savefig(name, dpi=300)
    plt.close()

# -------------------------------------------------------
# Fig 1: Overall Performance (Baseline Config)
# Config: 4KB Cache, 2-Way, 32B Block, FCFS (Priority 0)
# -------------------------------------------------------
subset_perf = df[
    (df['CacheSize'] == 4096) & 
    (df['Assoc'] == 2) &
    (df['BlockSize'] == 32) &
    (df['Bus_Priority'] == 0) & 
    (df['Protocol'].isin(['MESI', 'DRAGON', 'MOESI', 'MESIF']))
]

plt.figure(figsize=(10, 6))
sns.barplot(data=subset_perf, x='Benchmark', y='Cycles', hue='Protocol', palette='viridis')
plt.title('Figure 1: Baseline Performance (4KB Cache, 32B Block)')
plt.ylabel('Execution Cycles (Lower is Better)')
plt.yscale('log') # Log scale because benchmark lengths vary significantly
plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
save_fig('fig1_performance.png')

# -------------------------------------------------------
# Fig 2: Traffic Comparison (Baseline Config)
# MESI vs Dragon Traffic
# -------------------------------------------------------
subset_traffic = df[
    (df['CacheSize'] == 4096) & 
    (df['Bus_Priority'] == 0) & 
    (df['Protocol'].isin(['MESI', 'DRAGON']))
]

plt.figure(figsize=(10, 6))
sns.barplot(data=subset_traffic, x='Benchmark', y='Traffic', hue='Protocol', palette='magma')
plt.title('Figure 2: Bus Traffic Comparison (MESI vs Dragon)')
plt.ylabel('Total Traffic (Bytes)')
plt.yscale('log')
save_fig('fig2_traffic.png')

# -------------------------------------------------------
# Fig 3: Sensitivity to Cache Capacity
# Config: Bodytrack, MESI vs Dragon, Scaling Size (1024->8192)
# -------------------------------------------------------
cap_subset = df[
    (df['Benchmark'] == 'bodytrack') & 
    (df['Bus_Priority'] == 0) &
    (df['BlockSize'] == 32) & 
    (df['Protocol'].isin(['MESI', 'DRAGON']))
]

plt.figure(figsize=(10, 6))
sns.lineplot(data=cap_subset, x='CacheSize', y='Cycles', hue='Protocol', style='Protocol', marker='o', linewidth=2.5, markersize=8)
plt.title('Figure 3: Impact of Cache Capacity on Performance (Bodytrack)')
plt.ylabel('Execution Cycles')
plt.xlabel('Cache Size (Bytes)')
plt.xscale('log', base=2)
plt.grid(True, which="both", ls="--")
save_fig('fig3_capacity.png')

# -------------------------------------------------------
# Fig 4: False Sharing Analysis (Block Size)
# Config: Fluidanimate, 8KB Cache (Large), MESI
# Comparing 32B vs 64B Block Size
# -------------------------------------------------------
blk_subset = df[
    (df['Benchmark'] == 'fluidanimate') & 
    (df['CacheSize'] == 8192) &
    (df['Bus_Priority'] == 0) &
    (df['Protocol'] == 'MESI')
]

fig, ax1 = plt.subplots(figsize=(10, 6))
x = np.arange(len(blk_subset))
width = 0.35

# Plot Cycles on Left Y-Axis
ax1.bar(x - width/2, blk_subset['Cycles'], width, label='Execution Cycles', color='tab:blue')
ax1.set_ylabel('Execution Cycles', color='tab:blue', fontweight='bold')
ax1.set_title('Figure 4: Impact of Block Size (32B vs 64B) on Fluidanimate (MESI)')
ax1.set_xticks(x)
ax1.set_xticklabels(blk_subset['BlockSize'])
ax1.set_xlabel('Block Size (Bytes)')

# Plot Traffic on Right Y-Axis
ax2 = ax1.twinx()
ax2.bar(x + width/2, blk_subset['Traffic'], width, label='Bus Traffic', color='tab:red')
ax2.set_ylabel('Bus Traffic (Bytes)', color='tab:red', fontweight='bold')

# Legend
lines, labels = ax1.get_legend_handles_labels()
lines2, labels2 = ax2.get_legend_handles_labels()
ax1.legend(lines + lines2, labels + labels2, loc='upper center')
save_fig('fig4_blocksize.png')

# -------------------------------------------------------
# Fig 5: SMART Optimization Trade-off
# Config: Blackscholes, 4KB Cache
# -------------------------------------------------------
smart_subset = df[
    (df['Benchmark'] == 'blackscholes') & 
    (df['CacheSize'] == 4096) & 
    (df['Bus_Priority'] == 0) &
    (df['Protocol'].isin(['MESI', 'SMART']))
]

fig, ax1 = plt.subplots(figsize=(10, 6))
x = np.arange(len(smart_subset))

ax1.bar(x - width/2, smart_subset['Cycles'], width, label='Execution Cycles', color='gray')
ax1.set_ylabel('Execution Cycles (Lower is Better)', color='gray')
ax1.set_title('Figure 5: SMART Replacement: Writebacks vs Performance')
ax1.set_xticks(x)
ax1.set_xticklabels(smart_subset['Protocol'])

ax2 = ax1.twinx()
ax2.bar(x + width/2, smart_subset['Writebacks'], width, label='Bus Writebacks', color='green')
ax2.set_ylabel('Number of Writebacks (Lower is Better)', color='green')

lines, labels = ax1.get_legend_handles_labels()
lines2, labels2 = ax2.get_legend_handles_labels()
ax1.legend(lines + lines2, labels + labels2, loc='upper center')
save_fig('fig5_smart.png')

# -------------------------------------------------------
# Fig 6: Read Priority Optimization
# Config: Bodytrack, 8KB Cache (Large), MESI
# Comparing Priority 0 (FCFS) vs 1 (ReadFirst)
# -------------------------------------------------------
prio_subset = df[
    (df['Benchmark'] == 'bodytrack') & 
    (df['CacheSize'] == 8192) & 
    (df['BlockSize'] == 32) &
    (df['Protocol'] == 'MESI')
]

plt.figure(figsize=(8, 6))
sns.barplot(data=prio_subset, x='Bus_Priority', y='Cycles', palette='rocket')
plt.title('Figure 6: Impact of Read-Priority Arbitration (Bodytrack/MESI/8KB)')
plt.ylabel('Execution Cycles')
plt.xlabel('Bus Priority (0=FCFS, 1=Read-Priority)')
# Zoom in on Y-axis to show the difference clearly
low = prio_subset['Cycles'].min()
high = prio_subset['Cycles'].max()
plt.ylim(low * 0.98, high * 1.02)
save_fig('fig6_readpriority.png')

print("All figures generated from results_comprehensive.csv")