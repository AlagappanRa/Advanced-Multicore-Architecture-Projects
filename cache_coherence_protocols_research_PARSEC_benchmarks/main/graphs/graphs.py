import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

# Load Data
df = pd.read_csv("../results_comprehensive.csv")

# 1. Cache Scaling Analysis (Performance vs Size)
plt.figure(figsize=(10, 6))
subset = df[(df['Bus_Priority'] == 0) & (df['Protocol'] == 'MESI') & (df['BlockSize'] == 32)]
sns.barplot(data=subset, x='Benchmark', y='Cycles', hue='CacheSize')
plt.title('Impact of Cache Size on Performance (MESI)')
plt.ylabel('Execution Cycles (Lower is Better)')
plt.savefig('analysis_cache_size.png')
plt.show()

# 2. Block Size / False Sharing Analysis
plt.figure(figsize=(10, 6))
# Filter for larger caches where capacity misses aren't the noise
subset = df[(df['Bus_Priority'] == 0) & (df['CacheSize'] == 8192)]
sns.barplot(data=subset, x='Benchmark', y='Traffic', hue='BlockSize')
plt.title('Impact of Block Size on Bus Traffic (False Sharing Check)')
plt.ylabel('Total Traffic Bytes')
plt.savefig('analysis_block_size.png')
plt.show()

# 3. Read Priority Optimization Impact
plt.figure(figsize=(10, 6))
subset = df[(df['CacheSize'] == 4096) & (df['Protocol'] == 'MESI')] # Default config
sns.barplot(data=subset, x='Benchmark', y='Cycles', hue='Bus_Priority')
plt.title('Impact of Read-Priority Arbitration')
plt.ylabel('Execution Cycles')
plt.legend(title='Read Priority (0=FCFS, 1=ReadFirst)')
plt.savefig('analysis_priority.png')
plt.show()