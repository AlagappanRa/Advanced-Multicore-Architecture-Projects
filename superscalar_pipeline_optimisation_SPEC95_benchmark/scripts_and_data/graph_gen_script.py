import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

# Set academic style
plt.style.use('seaborn-v0_8-paper')
sns.set_context("paper", font_scale=1.4)
sns.set_style("whitegrid")

# ---------------------------------------------------------
# LOAD DATA (Based on the text provided in your prompt)
# ---------------------------------------------------------

# Part A Data (Out of Order) - top subset for visualization
df_ooo = pd.read_csv("partA_outorder_288.csv")
df_ooo['Type'] = 'Out-of-Order'

# Part B Data (In Order)
df_io = pd.read_csv("partB_inorder_64.csv")
df_io['Type'] = 'In-Order'

# Part C Data (Cache)
df_cache = pd.read_csv("partC_cache_optimization.csv")

# Combine for main plot
df_combined = pd.concat([df_ooo, df_io])

# ---------------------------------------------------------
# FIGURE 1: The Design Space Exploration (Pareto Frontier)
# ---------------------------------------------------------
plt.figure(figsize=(10, 6))

# Scatter plot
sns.scatterplot(data=df_combined, x='Area', y='IPC', hue='Type', style='Type', 
                palette={'Out-of-Order': '#2E86C1', 'In-Order': '#E74C3C'}, s=80, alpha=0.8)

# Add Constraint Line
plt.axvline(x=60, color='black', linestyle='--', label='Area Constraint (60 units)')

# Annotation for Best OOO
best_ooo = df_ooo[df_ooo['Area'] <= 60].sort_values('IPC', ascending=False).iloc[0]
plt.annotate(f'Best OOO\nIPC: {best_ooo.IPC:.2f}', 
             xy=(best_ooo.Area, best_ooo.IPC), xytext=(best_ooo.Area-15, best_ooo.IPC+0.05),
             arrowprops=dict(facecolor='black', shrink=0.05))

# Annotation for Efficiency Winner
best_io = df_io.sort_values('Efficiency', ascending=False).iloc[0]
plt.annotate(f'Efficiency Winner\nIPC/W: {best_io.Efficiency:.3f}', 
             xy=(best_io.Area, best_io.IPC), xytext=(best_io.Area+5, best_io.IPC-0.1),
             arrowprops=dict(facecolor='black', shrink=0.05))

plt.title('Design Space Exploration: IPC vs. Chip Area', fontweight='bold')
plt.xlabel('Estimated Chip Area (Units)')
plt.ylabel('Instructions Per Cycle (IPC)')
plt.legend(loc='lower right')
plt.tight_layout()
plt.savefig('fig1_design_space.png', dpi=300)
plt.close()

# ---------------------------------------------------------
# FIGURE 2: Efficiency Trade-off Analysis
# ---------------------------------------------------------
plt.figure(figsize=(8, 6))

# Prepare data for bar chart comparison
top_ooo = df_ooo[df_ooo['Area'] <= 60].sort_values('IPC', ascending=False).iloc[0]
top_io = df_io.sort_values('Efficiency', ascending=False).iloc[0]

comparison_data = {
    'Metric': ['Raw Performance (IPC)', 'Raw Performance (IPC)', 'Area Efficiency (IPC/W)', 'Area Efficiency (IPC/W)'],
    'Architecture': ['Out-of-Order (Optimized)', 'In-Order (Minimal)', 'Out-of-Order (Optimized)', 'In-Order (Minimal)'],
    'Value': [top_ooo.IPC, top_io.IPC, top_ooo.Efficiency * 10, top_io.Efficiency * 10] # Scale efficiency for visual comparison
}
df_comp = pd.DataFrame(comparison_data)

g = sns.barplot(data=df_comp, x='Metric', y='Value', hue='Architecture', palette={'Out-of-Order (Optimized)': '#2E86C1', 'In-Order (Minimal)': '#E74C3C'})
plt.title('Performance vs. Efficiency Trade-off', fontweight='bold')
plt.ylabel('Normalized Score')
plt.xlabel('')

# Add actual values on bars
for container in g.containers:
    g.bar_label(container, fmt='%.3f', padding=3)

plt.tight_layout()
plt.savefig('fig2_efficiency.png', dpi=300)
plt.close()

# ---------------------------------------------------------
# FIGURE 3: Cache Sensitivity
# ---------------------------------------------------------
plt.figure(figsize=(10, 5))

# Create a custom label for the cache config
df_cache['Label'] = "I:" + df_cache['ICache_Sets'].astype(str) + " / D:" + df_cache['DCache_Sets'].astype(str)

sns.barplot(data=df_cache, x='Label', y='IPC', color='#2ca02c')
plt.ylim(0.8, 1.15)
plt.title('Impact of Cache Asymmetry on Instruction Throughput', fontweight='bold')
plt.xlabel('Cache Sets (Instruction / Data)')
plt.ylabel('IPC')

# Highlight the inverse relationship
plt.annotate('Instruction Hunger', 
             xy=(0, 1.08), xytext=(2, 1.12),
             arrowprops=dict(facecolor='black', arrowstyle='->'))

plt.tight_layout()
plt.savefig('fig3_cache_optimization.png', dpi=300)
plt.close()

print("Graphs generated successfully.")