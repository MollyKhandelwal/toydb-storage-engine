import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv('obj1_stats.csv')

# We want to plot X-axis: mixtures of read/write queries
# Y-axis: physical I/O count
# Different lines for LRU and MRU and buffer sizes

plt.figure(figsize=(10, 6))

for strategy in ['LRU', 'MRU']:
    for buf_size in [10, 20, 50]:
        subset = df[(df['Strategy'] == strategy) & (df['BufferSize'] == buf_size)]
        if not subset.empty:
            plt.plot(subset['ReadMixPercent'], subset['PhysicalReads'] + subset['PhysicalWrites'], 
                     marker='o', label=f'{strategy} (Buf={buf_size})')

plt.title('Buffer Replacement Strategy Performance')
plt.xlabel('Read Queries (%)')
plt.ylabel('Total Physical I/O (Reads + Writes)')
plt.legend()
plt.grid(True)
plt.gca().invert_xaxis() # e.g. 100% reads down to 0% reads
plt.savefig('obj1_plot.png')
print("Plot saved to obj1_plot.png")
