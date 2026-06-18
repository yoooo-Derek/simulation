#!/usr/bin/env python3
"""Aggregate TL-HOC V7 community-main runs into seed statistics."""

import argparse
import csv
import math
import statistics
import sys
from collections import defaultdict
from pathlib import Path


SCHEMA_VERSION = "tl-hoc-v7"
SCHEMES = ("electrical-only", "static-ocs", "tl-hoc")
RHOS = (0.3, 0.5, 0.7, 0.9)
METRICS = (
    ("avg_fct_completed_only_s", "avg_fct_completed_only_ms", 1000.0),
    ("avg_fct_s", "avg_fct_ms", 1000.0),
    ("avg_receiver_throughput_bps", "avg_receiver_throughput_gbps", 1e-9),
    (
        "avg_receiver_throughput_installed_dest_bps",
        "avg_receiver_throughput_installed_dest_gbps",
        1e-9,
    ),
    ("total_received_bps", "total_received_gbps", 1e-9),
    (
        "avg_receiver_throughput_fraction_of_access_capacity",
        "avg_receiver_throughput_fraction_of_access_capacity",
        1.0,
    ),
    ("install_rate", "install_rate", 1.0),
    ("completion_rate_generated", "completion_rate_generated", 1.0),
    ("completion_rate_installed", "completion_rate_installed", 1.0),
    ("uninstalled_flows", "uninstalled_flows", 1.0),
    ("installed_incomplete_flows", "installed_incomplete_flows", 1.0),
    ("deferred_arrivals", "deferred_arrivals", 1.0),
    ("stage_boundary_blocked_count", "stage_boundary_blocked_count", 1.0),
)

OUTPUT_FIELDS = [
    "scheme",
    "rho",
    "metric",
    "seed_count",
    "metric_mean",
    "metric_std",
    "ci95_low",
    "ci95_high",
    "ci95_half_width",
    "ci95_half_width_ratio",
]


def expand_inputs(inputs):
    paths = []
    for value in inputs:
        path = Path(value)
        if path.is_dir():
            paths.extend(
                sorted(
                    candidate
                    for candidate in path.glob("*.csv")
                    if not candidate.name.endswith("-flows.csv")
                )
            )
        elif path.name.endswith("-flows.csv"):
            continue
        else:
            paths.append(path)
    return paths


def read_rows(paths):
    rows = []
    for path in paths:
        if not path.is_file():
            raise ValueError(f"input file does not exist: {path}")
        with path.open(newline="") as handle:
            reader = csv.DictReader(handle)
            for row_number, row in enumerate(reader, start=2):
                if row.get("schema_version") != SCHEMA_VERSION:
                    raise ValueError(
                        f"{path}:{row_number}: expected {SCHEMA_VERSION}, got "
                        f"{row.get('schema_version', '')}"
                    )
                rows.append((path, row_number, row))
    if not rows:
        raise ValueError("no summary rows found")
    return rows


def parse_float(path, row_number, row, field):
    value = row.get(field, "")
    if value == "":
        raise ValueError(f"{path}:{row_number}: {field} must not be empty")
    try:
        number = float(value)
    except ValueError as error:
        raise ValueError(f"{path}:{row_number}: {field} is not numeric: {value}") from error
    if not math.isfinite(number):
        raise ValueError(f"{path}:{row_number}: {field} is not finite: {value}")
    return number


def normalize_rho(value):
    for rho in RHOS:
        if abs(value - rho) < 1e-9:
            return rho
    raise ValueError(f"rho must be one of {RHOS}: {value}")


def build_stats(rows, min_seeds, allow_partial):
    grouped = defaultdict(dict)
    for path, row_number, row in rows:
        scheme = row.get("scheme", "")
        if scheme not in SCHEMES:
            raise ValueError(f"{path}:{row_number}: unexpected scheme: {scheme}")
        rho = normalize_rho(parse_float(path, row_number, row, "offered_load_factor"))
        seed = int(parse_float(path, row_number, row, "random_seed"))
        key = (scheme, rho, seed)
        if key in grouped:
            raise ValueError(f"{path}:{row_number}: duplicate scheme/rho/seed row: {key}")
        grouped[key] = row

    output_rows = []
    unstable_points = []
    observed_points = sorted({(scheme, rho) for scheme, rho, _ in grouped})
    expected_points = [(scheme, rho) for scheme in SCHEMES for rho in RHOS]
    points = observed_points if allow_partial else expected_points
    for scheme, rho in points:
        seed_rows = [row for (row_scheme, row_rho, _), row in grouped.items()
                     if row_scheme == scheme and row_rho == rho]
        if len(seed_rows) < min_seeds:
            raise ValueError(
                f"{scheme} rho={rho}: expected at least {min_seeds} seeds, "
                f"found {len(seed_rows)}"
            )
        for source_field, metric_name, scale in METRICS:
            values = [float(row[source_field]) * scale for row in seed_rows]
            mean = statistics.fmean(values)
            std = statistics.stdev(values) if len(values) > 1 else 0.0
            half_width = 1.96 * std / math.sqrt(len(values)) if len(values) > 1 else 0.0
            ratio = half_width / abs(mean) if mean != 0.0 else 0.0
            output_rows.append(
                {
                    "scheme": scheme,
                    "rho": f"{rho:.1f}",
                    "metric": metric_name,
                    "seed_count": str(len(values)),
                    "metric_mean": f"{mean:.12g}",
                    "metric_std": f"{std:.12g}",
                    "ci95_low": f"{mean - half_width:.12g}",
                    "ci95_high": f"{mean + half_width:.12g}",
                    "ci95_half_width": f"{half_width:.12g}",
                    "ci95_half_width_ratio": f"{ratio:.12g}",
                }
            )
            if ratio > 0.05:
                unstable_points.append((scheme, rho, metric_name, ratio, len(values)))
    return output_rows, unstable_points


def write_csv(path, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=OUTPUT_FIELDS)
        writer.writeheader()
        writer.writerows(rows)


def write_unstable(path, unstable_points):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["scheme", "rho", "metric", "ci95_half_width_ratio", "seed_count"])
        for scheme, rho, metric, ratio, seed_count in unstable_points:
            writer.writerow([scheme, f"{rho:.1f}", metric, f"{ratio:.12g}", seed_count])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", nargs="+", help="summary CSV files or raw result directories")
    parser.add_argument("--output", default="results/tables/v7-community-main-summary.csv")
    parser.add_argument("--min-seeds", type=int, default=10)
    parser.add_argument("--unstable-output", help="optional CSV path for CI half-width violations")
    parser.add_argument(
        "--require-expanded-if-unstable",
        action="store_true",
        help="fail if any point still exceeds 5% CI half-width with fewer than 20 seeds",
    )
    parser.add_argument(
        "--allow-partial",
        action="store_true",
        help="aggregate only observed scheme/rho points; intended for smoke runs",
    )
    args = parser.parse_args()

    try:
        paths = expand_inputs(args.inputs)
        if not paths:
            raise ValueError("no summary CSV inputs found")
        rows = read_rows(paths)
        output_rows, unstable_points = build_stats(rows, args.min_seeds, args.allow_partial)
        write_csv(Path(args.output), output_rows)
        if args.unstable_output:
            write_unstable(Path(args.unstable_output), unstable_points)
        if args.require_expanded_if_unstable:
            underexpanded = [point for point in unstable_points if point[4] < 20]
            if underexpanded:
                details = ", ".join(
                    f"{scheme}/rho={rho:.1f}/{metric}/n={seed_count}"
                    for scheme, rho, metric, _, seed_count in underexpanded
                )
                raise ValueError(f"unstable CI points need 20 seeds: {details}")
    except ValueError as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1

    print(f"PASS: wrote {len(output_rows)} statistical rows to {args.output}")
    if unstable_points:
        print(f"WARN: {len(unstable_points)} points exceed 5% CI half-width")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
