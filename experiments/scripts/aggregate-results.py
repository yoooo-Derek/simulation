#!/usr/bin/env python3
"""Combine TL-HOC V2 summary CSV rows into one deterministic summary table."""

import argparse
import csv
from pathlib import Path


OUTPUT_FIELDS = [
    "experiment",
    "scheme",
    "num_tors",
    "servers_per_tor",
    "traffic_pattern",
    "total_flows",
    "completed_flows",
    "avg_receiver_throughput_bps",
    "avg_fct_s",
    "avg_network_link_utilization",
]


def expand_inputs(inputs):
    paths = []
    for value in inputs:
        path = Path(value)
        if path.is_dir():
            paths.extend(sorted(candidate for candidate in path.glob("*.csv")
                                if not candidate.name.endswith("-flows.csv")))
        elif path.name.endswith("-flows.csv"):
            continue
        else:
            paths.append(path)
    return paths


def read_summary_rows(path):
    if not path.is_file():
        raise ValueError(f"input file does not exist: {path}")
    with path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        missing = [field for field in OUTPUT_FIELDS if field not in (reader.fieldnames or [])]
        if missing:
            raise ValueError(f"{path}: missing required fields: {','.join(missing)}")
        rows = list(reader)
    if not rows:
        raise ValueError(f"{path}: no summary rows")
    return [{field: row[field] for field in OUTPUT_FIELDS} for row in rows]


def main():
    parser = argparse.ArgumentParser(description="Aggregate TL-HOC V2 summary CSV rows.")
    parser.add_argument("inputs", nargs="+", help="summary CSV files or directories")
    parser.add_argument("--output", default="results/tables/summary-table.csv")
    args = parser.parse_args()

    paths = expand_inputs(args.inputs)
    if not paths:
        parser.error("no summary CSV inputs found")

    try:
        rows = []
        for path in paths:
            rows.extend(read_summary_rows(path))
    except ValueError as error:
        parser.error(str(error))

    rows.sort(key=lambda row: (row["scheme"], row["experiment"]))
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=OUTPUT_FIELDS)
        writer.writeheader()
        writer.writerows(rows)

    print(f"PASS: aggregated {len(rows)} summary rows into {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
