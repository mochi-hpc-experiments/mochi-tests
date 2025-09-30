#!/usr/bin/env python3

import sys
import pandas as pd
import matplotlib.pyplot as plt

input_file = sys.argv[1]
output_file = f"{input_file}.png"

try:
    data = pd.read_csv(input_file, sep='\t')

    x_col = data.columns[0]
    y_col = data.columns[4]

    x_data = data[x_col]
    y_data = data[y_col]

    plt.figure(figsize=(10, 6))
    plt.bar(x_data, y_data)

    plt.xlabel(x_col)
    plt.ylabel(y_col)
    plt.xticks(rotation=45, ha='right')
    plt.tight_layout()

    plt.savefig(output_file)

except (FileNotFoundError, IndexError) as e:
    print(f"Error processing file: {e}")
    sys.exit(1)
