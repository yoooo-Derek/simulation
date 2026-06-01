#!/usr/bin/env python3
"""List Phase 12B paper-plan manifest rows without running ns-3."""

import argparse
import csv
from pathlib import Path


FIELDS = [
    "experiment_name",
    "scheme",
    "topology_label",
    "num_tors",
    "servers_per_tor",
    "spines",
    "traffic_pattern",
    "load_label",
    "run_id",
    "random_seed",
    "config_file",
    "default_run",
]


def read_rows(path):
    if not path.is_file():
        raise ValueError(f"manifest does not exist: {path}")
    with path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        missing = [field for field in FIELDS if field not in (reader.fieldnames or [])]
        if missing:
            raise ValueError(f"{path}: missing required fields: {','.join(missing)}")
        return list(reader)


def print_table(rows):
    fields = [
        "experiment_name", "scheme", "topology_label", "num_tors",
        "servers_per_tor", "spines", "traffic_pattern", "load_label",
        "run_id", "random_seed", "default_run",
    ]
    widths = {field: max(len(field), *(len(row[field]) for row in rows)) for field in fields}
    print("  ".join(field.ljust(widths[field]) for field in fields))
    print("  ".join("-" * widths[field] for field in fields))
    for row in rows:
        print("  ".join(row[field].ljust(widths[field]) for field in fields))


def main():
    parser = argparse.ArgumentParser(description="List TL-OCS paper-plan draft rows.")
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--scheme")
    parser.add_argument("--topology-label")
    parser.add_argument("--traffic-pattern")
    parser.add_argument("--default-only", action="store_true")
    args = parser.parse_args()

    try:
        rows = read_rows(args.manifest)
    except ValueError as error:
        parser.error(str(error))

    if args.scheme:
        rows = [row for row in rows if row["scheme"] == args.scheme]
    if args.topology_label:
        rows = [row for row in rows if row["topology_label"] == args.topology_label]
    if args.traffic_pattern:
        rows = [row for row in rows if row["traffic_pattern"] == args.traffic_pattern]
    if args.default_only:
        rows = [row for row in rows if row["default_run"].lower() == "true"]

    rows.sort(key=lambda row: (row["topology_label"], row["scheme"], row["traffic_pattern"]))
    if rows:
        print_table(rows)
    else:
        print("No paper-plan rows matched.")
    print(f"Rows: {len(rows)}")
    print("Dry run only: no ns-3 command was executed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
