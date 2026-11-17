import csv
import os
import matplotlib.pyplot as plt

BASE = os.path.dirname(os.path.abspath(__file__))
SUMMARY = os.path.join(BASE, "benchmark_summary.csv")
RAW = os.path.join(BASE, "benchmark_results.csv")

with open(SUMMARY, newline="") as f:
    summary = list(csv.DictReader(f))

datasets = [1000, 5000, 10000]
workers = [1, 2, 3]

def get_value(dataset, worker, field):
    row = next(
        r for r in summary
        if int(r["dataset_records"]) == dataset
        and int(r["workers"]) == worker
    )
    return float(row[field])

# Graph 1: Mean execution time
plt.figure(figsize=(9, 5))
for dataset in datasets:
    values = [get_value(dataset, w, "mean_ms") for w in workers]
    plt.plot(workers, values, marker="o", label=f"{dataset:,} records")

plt.xlabel("Number of Workers")
plt.ylabel("Mean Execution Time (ms)")
plt.title("Mean Execution Time vs Worker Count")
plt.xticks(workers)
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.savefig(os.path.join(BASE, "mean_execution_time.png"), dpi=200)
plt.close()

# Graph 2: Median execution time
plt.figure(figsize=(9, 5))
for dataset in datasets:
    values = [get_value(dataset, w, "median_ms") for w in workers]
    plt.plot(workers, values, marker="o", label=f"{dataset:,} records")

plt.xlabel("Number of Workers")
plt.ylabel("Median Execution Time (ms)")
plt.title("Median Execution Time vs Worker Count")
plt.xticks(workers)
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.savefig(os.path.join(BASE, "median_execution_time.png"), dpi=200)
plt.close()

# Graph 3: Speedup relative to one worker
plt.figure(figsize=(9, 5))
for dataset in datasets:
    baseline = get_value(dataset, 1, "mean_ms")
    speedups = [
        baseline / get_value(dataset, w, "mean_ms")
        for w in workers
    ]
    plt.plot(workers, speedups, marker="o", label=f"{dataset:,} records")

plt.xlabel("Number of Workers")
plt.ylabel("Relative Speedup")
plt.title("Relative Speedup Compared with One Worker")
plt.xticks(workers)
plt.axhline(y=1, linestyle="--")
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.savefig(os.path.join(BASE, "speedup_comparison.png"), dpi=200)
plt.close()

# Generate report
lines = [
    "# SecureIPC - Benchmark Analysis Report",
    "",
    "## 1. Experiment Overview",
    "",
    "- Dataset sizes: 1,000, 5,000 and 10,000 records.",
    "- Worker configurations: 1, 2 and 3 workers.",
    "- Trials per configuration: 5.",
    "- Total benchmark runs: 45.",
    "- Metric: Internal analysis execution time in milliseconds.",
    "",
    "## 2. Benchmark Results",
    "",
    "| Records | Workers | Mean (ms) | Median (ms) | Min (ms) | Max (ms) |",
    "|---:|---:|---:|---:|---:|---:|"
]

for r in summary:
    lines.append(
        f'| {int(r["dataset_records"]):,} '
        f'| {r["workers"]} '
        f'| {float(r["mean_ms"]):.4f} '
        f'| {float(r["median_ms"]):.3f} '
        f'| {float(r["min_ms"]):.3f} '
        f'| {float(r["max_ms"]):.3f} |'
    )

lines += [
    "",
    "## 3. Graphs",
    "",
    "### Mean Execution Time",
    "",
    "![Mean Execution Time](mean_execution_time.png)",
    "",
    "### Median Execution Time",
    "",
    "![Median Execution Time](median_execution_time.png)",
    "",
    "### Relative Speedup",
    "",
    "![Speedup Comparison](speedup_comparison.png)",
    "",
    "## 4. Observations",
    ""
]

for dataset in datasets:
    values = {
        w: get_value(dataset, w, "mean_ms")
        for w in workers
    }
    best = min(values, key=values.get)
    lines.append(
        f"- For {dataset:,} records, {best} worker(s) had the "
        f"lowest observed mean execution time ({values[best]:.4f} ms)."
    )

lines += [
    "",
    "The results suggest that worker count affects execution time, "
    "but the effect depends on dataset size. More workers do not "
    "necessarily improve performance in every configuration.",
    "",
    "## 5. Conclusion",
    "",
    "The experiment demonstrates how different worker configurations "
    "affect SecureIPC dataset-processing performance. For the larger "
    "datasets tested, multiple workers generally performed better than "
    "a single worker. However, the best configuration varied by dataset.",
    "",
    "## 6. Measurement Limitations",
    "",
    "- Execution time measures the internal dataset-analysis operation.",
    "- Socket request setup and Flask dashboard overhead are excluded.",
    "- Process creation, synchronization and operating-system scheduling "
    "can affect measured results.",
    "- The measurements are very small, so caching and timing variation "
    "may influence the results.",
    "- Results are specific to this machine and test environment.",
    "",
    "## 7. Reproducibility",
    "",
    "Raw measurements: `benchmark_results.csv`",
    "",
    "Summary measurements: `benchmark_summary.csv`",
    "",
    "Individual run logs: `benchmark_logs/`",
    ""
]

with open(os.path.join(BASE, "BENCHMARK_REPORT.md"), "w") as f:
    f.write("\n".join(lines))

print("Benchmark report and graphs generated successfully.")
print("Files created:")
for name in [
    "mean_execution_time.png",
    "median_execution_time.png",
    "speedup_comparison.png",
    "BENCHMARK_REPORT.md"
]:
    print(" - results/" + name)
