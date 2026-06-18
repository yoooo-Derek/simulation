#!/usr/bin/env python3
"""Validate TL-HOC V7 community-main summary and per-flow CSV artifacts."""

import argparse
import csv
import math
import sys
from pathlib import Path


SCHEMA_VERSION = "tl-hoc-v7"
VALID_SCHEMES = {"electrical-only", "static-ocs", "tl-hoc"}
VALID_RHOS = {0.3, 0.5, 0.7, 0.9}
TRAFFIC_PATTERN = "community-local"

SUMMARY_FIELDS = {
    "schema_version",
    "experiment",
    "scheme",
    "traffic_pattern",
    "run_id",
    "random_seed",
    "status",
    "generated_flows",
    "installed_flows",
    "total_flows",
    "completed_flows",
    "avg_receiver_throughput_bps",
    "avg_fct_s",
    "offered_load_factor",
    "measurement_duration_s",
    "offered_bytes_measurement",
    "cross_tor_offered_bytes_measurement",
    "actual_cross_tor_offered_bps",
    "actual_received_bps",
    "normalized_eps_load",
}

FLOW_FIELDS = {
    "schema_version",
    "experiment",
    "scheme",
    "traffic_pattern",
    "run_id",
    "flow_id",
    "source_tor",
    "source_server",
    "destination_tor",
    "destination_server",
    "path_type",
    "size_bytes",
    "received_bytes",
    "start_time_s",
    "completion_time_s",
    "fct_s",
    "completed",
}

NONNEGATIVE_SUMMARY_FIELDS = {
    "total_flows",
    "generated_flows",
    "installed_flows",
    "completed_flows",
    "avg_receiver_throughput_bps",
    "avg_fct_s",
    "p90_fct_s",
    "p95_fct_s",
    "avg_network_link_utilization",
    "waiting_flows",
    "retried_flows",
    "interrupted_flows",
    "residual_flows",
    "ocs_flow_hit_rate",
    "ocs_byte_hit_rate",
    "offered_load_factor",
    "measurement_duration_s",
    "offered_bytes_measurement",
    "cross_tor_offered_bytes_measurement",
    "actual_offered_bps",
    "actual_cross_tor_offered_bps",
    "actual_received_bps",
    "normalized_access_load",
    "normalized_eps_load",
    "final_algorithm_candidate_edges",
    "final_algorithm_selected_edges",
    "final_ocs_active_edges",
    "cumulative_selected_edge_count",
    "avg_selected_edge_count",
    "max_selected_edge_count",
}


def require_fields(path, fieldnames, required):
    missing = sorted(required - set(fieldnames or []))
    if missing:
        raise ValueError(f"{path}: missing required fields: {','.join(missing)}")


def parse_number(path, row_number, field, value):
    try:
        number = float(value)
    except ValueError as error:
        raise ValueError(f"{path}:{row_number}: {field} is not numeric: {value}") from error
    if not math.isfinite(number):
        raise ValueError(f"{path}:{row_number}: {field} is not finite: {value}")
    return number


def parse_required_number(path, row_number, row, field):
    value = row.get(field, "")
    if value == "":
        raise ValueError(f"{path}:{row_number}: {field} must not be empty")
    return parse_number(path, row_number, field, value)


def parse_bool(value):
    return value.lower() in {"true", "1", "yes"}


def nearest_valid_rho(value):
    return min(VALID_RHOS, key=lambda rho: abs(rho - value))


def validate_common(path, row_number, row):
    if row.get("schema_version", "") != SCHEMA_VERSION:
        raise ValueError(
            f"{path}:{row_number}: unsupported schema_version: {row.get('schema_version', '')}"
        )
    if row.get("scheme", "") not in VALID_SCHEMES:
        raise ValueError(f"{path}:{row_number}: unsupported V7 scheme: {row.get('scheme', '')}")
    if row.get("traffic_pattern", "") != TRAFFIC_PATTERN:
        raise ValueError(
            f"{path}:{row_number}: traffic_pattern must be {TRAFFIC_PATTERN}: "
            f"{row.get('traffic_pattern', '')}"
        )


def validate_summary(path, fieldnames, rows, load_tolerance):
    require_fields(path, fieldnames, SUMMARY_FIELDS)
    for row_number, row in enumerate(rows, start=2):
        validate_common(path, row_number, row)
        rho = parse_required_number(path, row_number, row, "offered_load_factor")
        expected_rho = nearest_valid_rho(rho)
        if abs(rho - expected_rho) > 1e-9:
            raise ValueError(f"{path}:{row_number}: rho must be one of {sorted(VALID_RHOS)}: {rho}")

        normalized_eps_load = parse_required_number(path, row_number, row, "normalized_eps_load")
        tolerance = expected_rho * load_tolerance
        if abs(normalized_eps_load - expected_rho) > tolerance:
            raise ValueError(
                f"{path}:{row_number}: normalized_eps_load={normalized_eps_load} is outside "
                f"{expected_rho} +/- {tolerance}"
            )

        for field in NONNEGATIVE_SUMMARY_FIELDS:
            value = row.get(field, "")
            if value != "" and parse_number(path, row_number, field, value) < 0:
                raise ValueError(f"{path}:{row_number}: {field} must be non-negative")

        total = parse_required_number(path, row_number, row, "total_flows")
        generated = parse_required_number(path, row_number, row, "generated_flows")
        installed = parse_required_number(path, row_number, row, "installed_flows")
        completed = parse_required_number(path, row_number, row, "completed_flows")
        if total != generated:
            raise ValueError(f"{path}:{row_number}: total_flows must equal generated_flows")
        if installed > generated:
            raise ValueError(f"{path}:{row_number}: installed_flows exceeds generated_flows")
        if completed > total:
            raise ValueError(f"{path}:{row_number}: completed_flows exceeds total_flows")


def validate_flows(path, fieldnames, rows):
    require_fields(path, fieldnames, FLOW_FIELDS)
    for row_number, row in enumerate(rows, start=2):
        validate_common(path, row_number, row)
        received = parse_required_number(path, row_number, row, "received_bytes")
        if received < 0:
            raise ValueError(f"{path}:{row_number}: received_bytes must be non-negative")
        size = parse_required_number(path, row_number, row, "size_bytes")
        if size <= 0:
            raise ValueError(f"{path}:{row_number}: size_bytes must be positive")
        if row.get("path_type", "") == "":
            raise ValueError(f"{path}:{row_number}: path_type must not be empty")
        start_time = parse_required_number(path, row_number, row, "start_time_s")
        if start_time < 0:
            raise ValueError(f"{path}:{row_number}: start_time_s must be non-negative")
        completed = row.get("completed", "").lower()
        if completed not in {"true", "false"}:
            raise ValueError(f"{path}:{row_number}: completed must be true or false")
        fct = row.get("fct_s", "")
        if fct != "" and parse_number(path, row_number, "fct_s", fct) < 0:
            raise ValueError(f"{path}:{row_number}: fct_s must be non-negative")
        if parse_bool(completed) and (row.get("completion_time_s", "") == "" or fct == ""):
            raise ValueError(
                f"{path}:{row_number}: completed flow requires completion_time_s and fct_s"
            )


def validate_file(path, load_tolerance):
    if not path.is_file():
        raise ValueError(f"input file does not exist: {path}")
    with path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        rows = list(reader)
        fieldnames = reader.fieldnames or []
    if not rows:
        raise ValueError(f"{path}: no data rows")
    if SUMMARY_FIELDS.issubset(fieldnames):
        validate_summary(path, fieldnames, rows, load_tolerance)
        return "summary"
    if FLOW_FIELDS.issubset(fieldnames):
        validate_flows(path, fieldnames, rows)
        return "per-flow"
    raise ValueError(f"{path}: unrecognized TL-HOC CSV schema")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", nargs="+", help="summary or per-flow CSV files")
    parser.add_argument(
        "--load-tolerance",
        type=float,
        default=0.10,
        help="relative tolerance for normalized_eps_load around rho",
    )
    args = parser.parse_args()

    try:
        for value in args.inputs:
            path = Path(value)
            schema = validate_file(path, args.load_tolerance)
            print(f"PASS: {schema}: {path}")
    except ValueError as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
