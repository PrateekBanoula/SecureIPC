import csv
import re
import statistics
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CLIENT = ROOT / "secureipc_client"
OUT = ROOT / "results"
LOGS = OUT / "benchmark_logs"

DATASETS = {
    1000: "data/dataset_1000.csv",
    5000: "data/dataset_5000.csv",
    10000: "data/dataset_10000.csv",
}

WORKERS = [1, 2, 3]
TRIALS = 5

def run_client(command):
    result = subprocess.run(
        [str(CLIENT), command],
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=180
    )
    output = result.stdout + result.stderr

    if result.returncode != 0:
        raise RuntimeError(output)

    return output

results = []

print("\n========== SECUREIPC BENCHMARK ==========\n")

for records, dataset in DATASETS.items():
    for workers in WORKERS:
        print(f"\nDataset: {records} | Workers: {workers}")

        for trial in range(1, TRIALS + 1):
            config = run_client(f"CONFIG {dataset} {workers}")

            if "SUCCESS: Configuration updated." not in config:
                raise RuntimeError(f"Configuration failed:\n{config}")

            output = run_client("ANALYZE")

            duration = re.search(
                r"Analysis execution time:\s*([\d.]+)\s*ms",
                output
            )
            processed = re.search(
                r"Total records processed:\s*([\d,]+)",
                output
            )
            average = re.search(
                r"Average marks:\s*([\d.]+)",
                output
            )

            if not duration or not processed or \
               "SUCCESS: Dataset analysis completed." not in output:
                raise RuntimeError(
                    f"Trial failed: {records} records, "
                    f"{workers} workers, trial {trial}\n{output}"
                )

            execution_ms = float(duration.group(1))
            count = int(processed.group(1).replace(",", ""))
            avg_marks = float(average.group(1)) if average else None

            if count != records:
                raise RuntimeError(
                    f"Expected {records} records, got {count}"
                )

            entry = {
                "dataset_records": records,
                "dataset": dataset,
                "workers": workers,
                "trial": trial,
                "execution_ms": execution_ms,
                "records_processed": count,
                "average_marks": avg_marks,
                "status": "COMPLETED"
            }

            results.append(entry)

            log_file = LOGS / (
                f"{records}_records_{workers}_workers_trial_{trial}.txt"
            )
            log_file.write_text(output)

            print(f"  Trial {trial}: {execution_ms:.3f} ms")

# Save individual trial results.
raw_file = OUT / "benchmark_results.csv"

with raw_file.open("w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=results[0].keys())
    writer.writeheader()
    writer.writerows(results)

# Calculate grouped statistics.
summary = []

for records in DATASETS:
    for workers in WORKERS:
        values = [
            r["execution_ms"] for r in results
            if r["dataset_records"] == records
            and r["workers"] == workers
        ]

        summary.append({
            "dataset_records": records,
            "workers": workers,
            "trials": len(values),
            "mean_ms": round(statistics.mean(values), 4),
            "median_ms": round(statistics.median(values), 4),
            "min_ms": round(min(values), 4),
            "max_ms": round(max(values), 4),
        })

summary_file = OUT / "benchmark_summary.csv"

with summary_file.open("w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=summary[0].keys())
    writer.writeheader()
    writer.writerows(summary)

print("\n========== BENCHMARK SUMMARY ==========")
print("Dataset       Workers   Mean (ms)   Median (ms)")

for row in summary:
    print(
        f'{row["dataset_records"]:>7}       '
        f'{row["workers"]:>3}       '
        f'{row["mean_ms"]:>8.4f}     '
        f'{row["median_ms"]:>8.4f}'
    )

print("\nAll 45 runs completed successfully.")
print(f"Raw results: {raw_file}")
print(f"Summary:     {summary_file}")
print(f"Logs:        {LOGS}")
