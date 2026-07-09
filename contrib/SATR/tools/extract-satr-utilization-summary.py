#!/usr/bin/env python3
import argparse
import csv
from pathlib import Path


OUTPUT_FIELDS = [
    "offeredLoad",
    "strategy",
    "avgLinkUtilization",
]


def resolve_summary_path(input_path: Path) -> Path:
    if input_path.is_dir():
        return input_path / "summary.csv"
    return input_path


def main():
    parser = argparse.ArgumentParser(description="Extract SATR average link utilization summary.")
    parser.add_argument("input", type=Path, help="Experiment output directory or summary.csv")
    parser.add_argument(
        "--output",
        type=Path,
        help="Output CSV path. Defaults to <experiment-dir>/utilization-summary.csv.",
    )
    args = parser.parse_args()

    summary_path = resolve_summary_path(args.input)
    if not summary_path.exists():
        raise SystemExit(f"missing summary.csv: {summary_path}")
    output_path = args.output or (summary_path.parent / "utilization-summary.csv")

    with summary_path.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))

    required = [
        "offeredLoad",
        "strategy",
        "avgLinkUtilization",
    ]
    missing = [field for field in required if rows and field not in rows[0]]
    if missing:
        raise SystemExit(f"summary.csv is missing required fields: {', '.join(missing)}")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=OUTPUT_FIELDS, lineterminator="\n")
        writer.writeheader()
        for row in sorted(rows, key=lambda r: (float(r["offeredLoad"]), r["strategy"])):
            writer.writerow(
                {
                    "offeredLoad": row["offeredLoad"],
                    "strategy": row["strategy"],
                    "avgLinkUtilization": row["avgLinkUtilization"],
                }
            )

    print(f"rows={len(rows)} output={output_path}")


if __name__ == "__main__":
    main()
