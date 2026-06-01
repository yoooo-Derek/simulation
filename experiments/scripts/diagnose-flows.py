#!/usr/bin/env python3
"""Report incomplete TL-OCS flows without modifying result artifacts."""

import argparse
import csv
import math
import statistics
from collections import Counter, defaultdict
from pathlib import Path


FLOW_FIELDS = {
    "experiment", "scheme", "flow_id", "source_tor", "destination_tor",
    "path_type", "start_time_s", "received_bytes", "fct_s", "completed",
}
SUMMARY_FIELDS = {"experiment", "completed_flows", "stop_time_s"}
OUTPUT_FIELDS = [
    "input_file",
    "scheme",
    "path_type",
    "source_tor",
    "destination_tor",
    "total_flows",
    "completed_flows",
    "incomplete_flows",
    "avg_fct_s",
    "p90_fct_s",
    "p95_fct_s",
]


def require_fields(path, fieldnames, required):
    missing = sorted(required - set(fieldnames or []))
    if missing:
        raise ValueError(f"{path}: missing required fields: {','.join(missing)}")


def read_csv(path, required):
    if not path.is_file():
        raise ValueError(f"input file does not exist: {path}")
    with path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        require_fields(path, reader.fieldnames, required)
        rows = list(reader)
    if not rows:
        raise ValueError(f"{path}: no data rows")
    return rows


def is_completed(row):
    value = row["completed"].lower()
    if value not in {"true", "false"}:
        raise ValueError(f"invalid completed value for flow {row['flow_id']}: {row['completed']}")
    return value == "true"


def parse_float(value, field, context):
    try:
        return float(value)
    except ValueError as error:
        raise ValueError(f"{context}: {field} is not numeric: {value}") from error


def nearest_rank(values, percentile):
    if not values:
        return None
    ordered = sorted(values)
    index = max(0, math.ceil(percentile * len(ordered)) - 1)
    return ordered[index]


def format_number(value):
    return "" if value is None else f"{value:.12g}"


def completed_fcts(rows):
    values = []
    for row in rows:
        if is_completed(row):
            if row["fct_s"] == "":
                raise ValueError(f"completed flow {row['flow_id']} has empty fct_s")
            values.append(parse_float(row["fct_s"], "fct_s", f"flow {row['flow_id']}"))
    return values


def stats(rows):
    fcts = completed_fcts(rows)
    return {
        "total": len(rows),
        "completed": len(fcts),
        "incomplete": len(rows) - len(fcts),
        "min": min(fcts) if fcts else None,
        "avg": statistics.fmean(fcts) if fcts else None,
        "max": max(fcts) if fcts else None,
        "p90": nearest_rank(fcts, 0.90),
        "p95": nearest_rank(fcts, 0.95),
    }


def read_summaries(paths):
    summaries = {}
    for path in paths:
        for row in read_csv(path, SUMMARY_FIELDS):
            summaries[row["experiment"]] = row
    return summaries


def counter_text(counter):
    return ", ".join(f"{key}={count}" for key, count in sorted(counter.items())) or "none"


def print_report(path, rows, summary):
    result = stats(rows)
    incomplete = [row for row in rows if not is_completed(row)]
    print(f"FLOW DIAGNOSTIC: {path}")
    print(f"  total={result['total']} completed={result['completed']} incomplete={result['incomplete']}")
    print("  completed FCT: "
          f"min={format_number(result['min'])} avg={format_number(result['avg'])} "
          f"max={format_number(result['max'])} p90={format_number(result['p90'])} "
          f"p95={format_number(result['p95'])}")
    print(f"  incomplete by scheme: {counter_text(Counter(row['scheme'] for row in incomplete))}")
    print(f"  incomplete by path_type: {counter_text(Counter(row['path_type'] for row in incomplete))}")
    print(f"  incomplete by source_tor: {counter_text(Counter(row['source_tor'] for row in incomplete))}")
    print(f"  incomplete by destination_tor: {counter_text(Counter(row['destination_tor'] for row in incomplete))}")
    pairs = Counter(f"{row['source_tor']}->{row['destination_tor']}" for row in incomplete)
    print(f"  incomplete by pair: {counter_text(pairs)}")
    if incomplete:
        starts = [parse_float(row["start_time_s"], "start_time_s", f"flow {row['flow_id']}")
                  for row in incomplete]
        print(f"  incomplete start range: {min(starts):.12g}..{max(starts):.12g}")
    if summary is not None:
        expected = int(summary["completed_flows"])
        if expected != result["completed"]:
            raise ValueError(
                f"{path}: completed flow count {result['completed']} does not match summary {expected}"
            )
        stop_time = parse_float(summary["stop_time_s"], "stop_time_s", summary["experiment"])
        starts_after_stop = sum(
            parse_float(row["start_time_s"], "start_time_s", f"flow {row['flow_id']}") >= stop_time
            for row in incomplete
        )
        print(f"  summary completed count matches: {expected}")
        print(f"  incomplete starts at/after stop_time_s={stop_time:.12g}: {starts_after_stop}")


def diagnostic_rows(path, rows):
    grouped = defaultdict(list)
    for row in rows:
        grouped[(row["scheme"], row["path_type"], row["source_tor"], row["destination_tor"])].append(row)
    output = []
    for (scheme, path_type, source_tor, destination_tor), group in sorted(grouped.items()):
        result = stats(group)
        output.append({
            "input_file": str(path),
            "scheme": scheme,
            "path_type": path_type,
            "source_tor": source_tor,
            "destination_tor": destination_tor,
            "total_flows": str(result["total"]),
            "completed_flows": str(result["completed"]),
            "incomplete_flows": str(result["incomplete"]),
            "avg_fct_s": format_number(result["avg"]),
            "p90_fct_s": format_number(result["p90"]),
            "p95_fct_s": format_number(result["p95"]),
        })
    return output


def main():
    parser = argparse.ArgumentParser(description="Diagnose incomplete TL-OCS per-flow CSV rows.")
    parser.add_argument("inputs", nargs="+", help="per-flow CSV files")
    parser.add_argument("--summary", nargs="*", default=[], help="optional matching summary CSV files")
    parser.add_argument("--output", help="optional diagnostic CSV output")
    args = parser.parse_args()

    try:
        summaries = read_summaries([Path(value) for value in args.summary])
        output_rows = []
        for value in args.inputs:
            path = Path(value)
            rows = read_csv(path, FLOW_FIELDS)
            experiments = {row["experiment"] for row in rows}
            if len(experiments) != 1:
                raise ValueError(f"{path}: expected exactly one experiment, found {len(experiments)}")
            experiment = next(iter(experiments))
            print_report(path, rows, summaries.get(experiment))
            output_rows.extend(diagnostic_rows(path, rows))
        if args.output:
            output = Path(args.output)
            output.parent.mkdir(parents=True, exist_ok=True)
            with output.open("w", newline="") as handle:
                writer = csv.DictWriter(handle, fieldnames=OUTPUT_FIELDS)
                writer.writeheader()
                writer.writerows(output_rows)
            print(f"PASS: wrote {len(output_rows)} diagnostic rows to {output}")
    except ValueError as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
