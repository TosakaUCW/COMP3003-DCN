import matplotlib.pyplot as plt
import numpy as np
from matplotlib.pyplot import MultipleLocator


# Define the bit-stream
bitstream = "11011010"  # "1101 1010" with spaces removed

# Set parameters: each bit occupies 1 unit time (split into 2 sub-intervals)
bit_duration = 1.0
half_bit = bit_duration / 2.0

# Initialize lists to hold time and signal level values.
times = []
levels = []

# Start time
t = 0.0

# Loop over each bit in the bitstream:
for bit in bitstream:
    # For each bit, we create three time points:
    #  t (start), t+half_bit (midpoint), and t+bit_duration (end)
    # Manchester encoding:
    #   for bit '1': low (0) for first half, then high (1) for second half.
    #   for bit '0': high (1) for first half, then low (0) for second half.
    if bit == '1':
        # Start at low (0), then jump to high (1)
        times.extend([t, t + half_bit, t + half_bit, t + bit_duration])
        levels.extend([1, 1, 0, 0])
    else:  # bit == '0'
        # Start at high (1), then jump to low (0)
        times.extend([t, t + half_bit, t + half_bit, t + bit_duration])
        levels.extend([0, 0, 1, 1])
    t += bit_duration

# To create proper step-like transitions, we use plt.step() with the where parameter.
plt.figure(figsize=(12, 2.5))
plt.step(times, levels, where='post', linewidth=2)

ax = plt.gca()
ax.yaxis.set_major_locator(MultipleLocator(1))
plt.ylim(-0.5, 1.5)
plt.xlim(0, t)
plt.xlabel("Time")
plt.ylabel("Signal Level")

plt.grid(True)

# Annotate bit values for clarity
bit_timestamps = np.arange(0.5, t, step=1)
for i, (bt, ts) in enumerate(zip(bitstream, bit_timestamps)):
    plt.text(ts, 1.6, bt, ha='center', fontsize=12)


# plt.title("Manchester Encoding for Bit-Stream '1101 1010'")

plt.show()
