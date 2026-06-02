#!/usr/bin/env python3
"""Validate TL-OCS summary and per-flow CSV artifacts."""

import argparse
import csv
import sys
from pathlib import Path


SUMMARY_BASE_FIELDS = {
    "experiment", "scheme", "status", "installed_flows", "received_bytes"
}
FLOW_FIELDS = {
    "flow_id", "path_type", "received_bytes", "completed"
}
FLOW_PATH_TYPES = {"eps", "ocs"}
NONNEGATIVE_SUMMARY_FIELDS = {
    "installed_flows", "received_bytes", "total_flows", "completed_flows",
    "incomplete_flows", "eps_avg_link_utilization", "eps_max_link_utilization",
    "ocs_avg_link_utilization", "ocs_max_link_utilization",
    "ocs_reconfiguration_count",
}
HIT_RATE_FIELDS = {"ocs_flow_hit_rate", "ocs_byte_hit_rate"}


def require_fields(path, fieldnames, required):
    missing = sorted(required - set(fieldnames or []))
    if missing:
        raise ValueError(f"{path}: missing required fields: {','.join(missing)}")


def parse_number(path, row_number, field, value):
    try:
        return float(value)
    except ValueError as error:
        raise ValueError(f"{path}:{row_number}: {field} is not numeric: {value}") from error


def validate_summary(path, fieldnames, rows):
    require_fields(path, fieldnames, SUMMARY_BASE_FIELDS)
    for row_number, row in enumerate(rows, start=2):
        experiment = row.get("experiment", "")
        metrics_summary = experiment.startswith(("phase11a-", "phase11b-", "sanity-"))
        util_summary = experiment.startswith(("phase11b-", "sanity-"))
        if metrics_summary:
            require_fields(path, fieldnames, {"total_flows", "completed_flows", "avg_fct_s"})
        if util_summary:
            require_fields(path,
                           fieldnames,
                           {"eps_avg_link_utilization", "eps_max_link_utilization"})
            if row.get("scheme", "") in {"ocs-volume", "ocs-community", "tl-ocs"}:
                require_fields(path, fieldnames, {"ocs_flow_hit_rate"})

        required_numbers = {"installed_flows", "received_bytes"}
        if metrics_summary:
            required_numbers.update({"total_flows", "completed_flows"})
        for field in required_numbers:
            if row.get(field, "") == "":
                raise ValueError(f"{path}:{row_number}: {field} must not be empty")

        for field in NONNEGATIVE_SUMMARY_FIELDS:
            value = row.get(field, "")
            if value != "" and parse_number(path, row_number, field, value) < 0:
                raise ValueError(f"{path}:{row_number}: {field} must be non-negative")
        for field in HIT_RATE_FIELDS:
            value = row.get(field, "")
            if value != "":
                number = parse_number(path, row_number, field, value)
                if number < 0 or number > 1:
                    raise ValueError(f"{path}:{row_number}: {field} must be in [0,1]")

        total = row.get("total_flows", "")
        completed = row.get("completed_flows", "")
        if total != "" and completed != "":
            if parse_number(path, row_number, "completed_flows", completed) > \
                    parse_number(path, row_number, "total_flows", total):
                raise ValueError(f"{path}:{row_number}: completed_flows exceeds total_flows")


def validate_flows(path, fieldnames, rows):
    require_fields(path, fieldnames, FLOW_FIELDS)
    for row_number, row in enumerate(rows, start=2):
        if row.get("path_type", "") not in FLOW_PATH_TYPES:
            raise ValueError(f"{path}:{row_number}: unsupported path_type: {row.get('path_type', '')}")
        received = row.get("received_bytes", "")
        if received == "" or parse_number(path, row_number, "received_bytes", received) < 0:
            raise ValueError(f"{path}:{row_number}: received_bytes must be non-negative")
        completed = row.get("completed", "").lower()
        if completed not in {"true", "false"}:
            raise ValueError(f"{path}:{row_number}: completed must be true or false")
        fct = row.get("fct_s", "")
        if fct != "" and parse_number(path, row_number, "fct_s", fct) < 0:
            raise ValueError(f"{path}:{row_number}: fct_s must be non-negative")
        if completed == "true":
            if row.get("completion_time_s", "") == "" or fct == "":
                raise ValueError(f"{path}:{row_number}: completed flow requires completion_time_s and fct_s")


def validate_file(path):
    if not path.is_file():
        raise ValueError(f"input file does not exist: {path}")
    with path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        rows = list(reader)
        fieldnames = reader.fieldnames or []
    if not rows:
        raise ValueError(f"{path}: no data rows")
    if SUMMARY_BASE_FIELDS.issubset(fieldnames):
        validate_summary(path, fieldnames, rows)
        return "summary"
    if FLOW_FIELDS.issubset(fieldnames):
        validate_flows(path, fieldnames, rows)
        return "per-flow"
    raise ValueError(f"{path}: unrecognized TL-OCS CSV schema")


def main():
    parser = argparse.ArgumentParser(description="Validate TL-OCS result CSV files.")
    parser.add_argument("inputs", nargs="+", help="summary or per-flow CSV files")
    args = parser.parse_args()

    try:
        for value in args.inputs:
            path = Path(value)
            schema = validate_file(path)
            print(f"PASS: {schema}: {path}")
    except ValueError as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
