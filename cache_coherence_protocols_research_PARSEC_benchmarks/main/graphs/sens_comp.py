import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np

# Load Data
df = pd.read_csv('../results_comprehensive.csv')

# Set visual style
sns.set_theme(style="whitegrid")
plt.rcParams.update({'font.size': 11})

# Helper function to save plots
def save_plot(filename):
    plt.tight_layout()
    plt.savefig(filename, dpi=300)
    plt.close()

# ---------------------------------------------------------
# 1. Protocol Sensitivity to Cache Size (Capacity Analysis)
# QUESTION: As we eliminate capacity misses (larger cache), 
# does the gap between MESI and Dragon widen or close?
# ---------------------------------------------------------
# Filter: Bodytrack, Priority 0, Block 32, Assoc varies with size
cap_df = df[
    (df['Benchmark'] == 'bodytrack') & 
    (df['Bus_Priority'] == 0) &
    (df['BlockSize'] == 32) &
    (df['Protocol'].isin(['MESI', 'DRAGON']))
]

plt.figure(figsize=(8, 6))
sns.lineplot(data=cap_df, x='CacheSize', y='Cycles', hue='Protocol', style='Protocol', markers=True, linewidth=2.5, markersize=9)
plt.title('Protocol Sensitivity to Cache Capacity (Bodytrack)')
plt.ylabel('Execution Cycles')
plt.xlabel('L1 Cache Size (Bytes)')
plt.xscale('log', base=2)
# Invert Y axis logically? No, lower is better.
plt.grid(True, which="both", ls="--")
save_plot('analysis_capacity_sensitivity.png')

# ---------------------------------------------------------
# 2. False Sharing Analysis (Block Size Sensitivity)
# QUESTION: Does Dragon handle False Sharing (64B blocks) better 
# than MESI due to word-level updates?
# ---------------------------------------------------------
# Filter: Fluidanimate (high contention), Large Cache (8192) to isolate coherence
fs_df = df[
    (df['Benchmark'] == 'fluidanimate') & 
    (df['CacheSize'] == 8192) &
    (df['Bus_Priority'] == 0) &
    (df['Protocol'].isin(['MESI', 'DRAGON']))
]

plt.figure(figsize=(8, 6))
chart = sns.barplot(data=fs_df, x='BlockSize', y='Traffic', hue='Protocol', palette='muted')
plt.title('Impact of Block Size on Bus Traffic (False Sharing)')
plt.ylabel('Total Bus Traffic (Bytes)')
plt.xlabel('Cache Block Size (Bytes)')
save_plot('analysis_falsesharing.png')

# ---------------------------------------------------------
# 3. Optimization Robustness (MOESI vs MESI across sizes)
# QUESTION: Does MOESI provide benefits only at specific cache sizes?
# ---------------------------------------------------------
# Filter: Bodytrack
moesi_df = df[
    (df['Benchmark'] == 'bodytrack') & 
    (df['Bus_Priority'] == 0) &
    (df['BlockSize'] == 32) &
    (df['Protocol'].isin(['MESI', 'MOESI']))
]

plt.figure(figsize=(8, 6))
# Calculate % improvement of MOESI over MESI
# (This might be 0, which is a valid research finding)
sns.barplot(data=moesi_df, x='CacheSize', y='Cycles', hue='Protocol', palette='Paired')
plt.title('MOESI vs MESI Stability Across Cache Sizes')
plt.ylabel('Execution Cycles')
# Zoom in to show micro-differences if any
plt.ylim(moesi_df['Cycles'].min() * 0.9, moesi_df['Cycles'].max() * 1.05)
save_plot('analysis_moesi_robustness.png')

# ---------------------------------------------------------
# 4. Replacement Policy Trade-off (The "Smart" Analysis)
# QUESTION: How does the "Smart" policy affect the Hit Rate vs Writeback count?
# ---------------------------------------------------------
# Filter: Blackscholes, 4096 size
smart_df = df[
    (df['Benchmark'] == 'blackscholes') & 
    (df['CacheSize'] == 4096) &
    (df['Protocol'].isin(['MESI', 'SMART']))
]

fig, ax1 = plt.subplots(figsize=(8, 6))

x = np.arange(len(smart_df))
width = 0.35

# Plot Cycles (Performance) on Left Y
ax1.bar(x - width/2, smart_df['Cycles'], width, label='Execution Cycles', color='tab:red', alpha=0.7)
ax1.set_ylabel('Execution Cycles (Lower is Better)', color='tab:red')
ax1.tick_params(axis='y', labelcolor='tab:red')
ax1.set_ylim(0, smart_df['Cycles'].max() * 1.2)

# Plot Writebacks on Right Y
ax2 = ax1.twinx()
ax2.bar(x + width/2, smart_df['Writebacks'], width, label='Writebacks', color='tab:blue', alpha=0.7)
ax2.set_ylabel('Bus Writebacks (Lower is Better)', color='tab:blue')
ax2.tick_params(axis='y', labelcolor='tab:blue')
ax2.set_ylim(0, smart_df['Writebacks'].max() * 1.2)

plt.title('The Replacement Policy Trade-off: Cycles vs Writebacks')
ax1.set_xticks(x)
ax1.set_xticklabels(smart_df['Protocol'])
save_plot('analysis_smart_tradeoff.png')

print("Research graphs generated.")