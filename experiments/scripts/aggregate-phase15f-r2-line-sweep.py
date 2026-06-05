#!/usr/bin/env python3
"""Aggregate Phase 15F-R2 offered-load line-sweep pilot results."""

from __future__ import annotations

import argparse
import csv
import math
import re
import statistics
from collections import defaultdict
from pathlib import Path


SCHEMES = ("eps-ecmp", "ocs-volume", "tl-ocs")

RUN_RE = re.compile(
    r"^phase15f-r2-(?P<scenario>.+)-(?P<scheme>eps-ecmp|ocs-volume|tl-ocs)-"
    r"seed(?P<seed>\d+)-load(?P<load>[0-9]+p[0-9]+)-"
    r"k(?P<k>\d+)-thetaF(?P<thetaf>\d+)-agg(?P<agg>\d+)"
    r"(?P<flows>-flows)?\.csv$"
)

SUMMARY_METRICS = (
    "completed_flows",
    "incomplete_flows",
    "avg_received_throughput_bps",
    "avg_fct_s",
    "p90_fct_s",
    "p95_fct_s",
    "ocs_assigned_flows",
    "eps_fallback_flows",
    "ocs_flow_hit_rate",
    "ocs_byte_hit_rate",
    "eps_avg_link_utilization",
    "eps_max_link_utilization",
    "ocs_avg_link_utilization",
    "ocs_max_link_utilization",
    "avg_active_edge_count",
    "non_empty_scheduling_rounds",
)

FLOW_SEQUENCE_FIELDS = (
    "source_tor",
    "source_server",
    "destination_tor",
    "destination_server",
    "size_bytes",
    "start_time_s",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raw-dir", default="results/raw", type=Path)
    parser.add_argument("--processed-dir", default="results/processed", type=Path)
    return parser.parse_args()


def parse_name(path: Path) -> dict[str, object]:
    match = RUN_RE.match(path.name)
    if not match:
        raise ValueError(f"Unexpected Phase 15F-R2 file name: {path}")
    groups = match.groupdict()
    return {
        "scenario": groups["scenario"],
        "scheme": groups["scheme"],
        "seed": int(groups["seed"]),
        "offered_load_factor": float(groups["load"].replace("p", ".")),
        "k": int(groups["k"]),
        "thetaF": int(groups["thetaf"]),
        "aggregatorCount": int(groups["agg"]),
        "is_flow": bool(groups["flows"]),
    }


def read_summary(path: Path) -> dict[str, str]:
    with path.open(newline="") as handle:
        rows = list(csv.DictReader(handle))
    if len(rows) != 1:
        raise ValueError(f"Expected one summary row in {path}, found {len(rows)}")
    return rows[0]


def read_flows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as handle:
        return list(csv.DictReader(handle))


def to_float(value: str | None) -> float:
    if value is None or value == "":
        return float("nan")
    try:
        return float(value)
    except ValueError:
        return float("nan")


def fmt(value: object) -> str:
    if isinstance(value, float):
        if math.isnan(value):
            return ""
        return f"{value:.12g}"
    return str(value)


def mean(values: list[float]) -> float:
    clean = [value for value in values if not math.isnan(value)]
    return statistics.fmean(clean) if clean else float("nan")


def sample_std(values: list[float]) -> float:
    clean = [value for value in values if not math.isnan(value)]
    if len(clean) < 2:
        return float("nan")
    return statistics.stdev(clean)


def discover(raw_dir: Path) -> tuple[dict[tuple, Path], dict[tuple, Path]]:
    summaries: dict[tuple, Path] = {}
    flows: dict[tuple, Path] = {}
    for path in sorted(raw_dir.glob("phase15f-r2-*.csv")):
        parsed = parse_name(path)
        key = (
            parsed["scenario"],
            parsed["scheme"],
            parsed["seed"],
            parsed["offered_load_factor"],
            parsed["k"],
            parsed["thetaF"],
            parsed["aggregatorCount"],
        )
        if parsed["is_flow"]:
            flows[key] = path
        else:
            summaries[key] = path
    return summaries, flows


def validate(
    summaries: dict[tuple, Path],
    flows: dict[tuple, Path],
) -> tuple[dict[tuple, dict[str, str]], dict[tuple, list[dict[str, str]]], dict[str, str]]:
    summary_rows: dict[tuple, dict[str, str]] = {}
    flow_rows: dict[tuple, list[dict[str, str]]] = {}
    errors: list[str] = []
    warnings: list[str] = []

    if not summaries:
        errors.append("no phase15f-r2 summary CSV files found")

    for key, summary_path in summaries.items():
        if key not in flows:
            errors.append(f"missing flow CSV for {summary_path}")
            continue
        row = read_summary(summary_path)
        flow_data = read_flows(flows[key])
        summary_rows[key] = row
        flow_rows[key] = flow_data

        total = int(to_float(row.get("total_flows", "0")))
        installed = int(to_float(row.get("installed_flows", "0")))
        completed = int(to_float(row.get("completed_flows", "0")))
        incomplete = int(to_float(row.get("incomplete_flows", "0")))
        if total != installed:
            errors.append(f"{summary_path}: total_flows != installed_flows")
        if completed + incomplete != total:
            errors.append(f"{summary_path}: completed + incomplete != total")
        if len(flow_data) != total:
            errors.append(f"{flows[key]}: row count {len(flow_data)} != total_flows {total}")
        if to_float(row.get("received_bytes")) <= 0:
            errors.append(f"{summary_path}: received_bytes is not positive")
        if to_float(row.get("avg_received_throughput_bps")) <= 0:
            errors.append(f"{summary_path}: avg_received_throughput_bps is not positive")
        for metric in ("ocs_flow_hit_rate", "ocs_byte_hit_rate"):
            value = to_float(row.get(metric))
            if not math.isnan(value) and not (0.0 <= value <= 1.0):
                errors.append(f"{summary_path}: {metric} out of [0,1]")
        for metric in (
            "eps_avg_link_utilization",
            "eps_max_link_utilization",
            "ocs_avg_link_utilization",
            "ocs_max_link_utilization",
        ):
            value = to_float(row.get(metric))
            if math.isnan(value) or value < 0:
                errors.append(f"{summary_path}: invalid {metric}")
        if key[1] == "eps-ecmp":
            if int(to_float(row.get("ocs_assigned_flows", "0"))) != 0:
                errors.append(f"{summary_path}: EPS-only run has OCS assignments")
            if to_float(row.get("ocs_flow_hit_rate")) != 0:
                errors.append(f"{summary_path}: EPS-only run has nonzero OCS flow hit rate")
            if to_float(row.get("ocs_avg_link_utilization")) != 0:
                errors.append(f"{summary_path}: EPS-only run has nonzero OCS utilization")
        for flow in flow_data:
            if flow.get("path_type") not in {"eps", "ocs"}:
                errors.append(f"{flows[key]}: invalid path_type {flow.get('path_type')}")
            if flow.get("completed") == "true":
                if int(to_float(flow.get("received_bytes"))) != int(to_float(flow.get("size_bytes"))):
                    errors.append(f"{flows[key]}: completed flow has received_bytes != size_bytes")
                if to_float(flow.get("fct_s")) < 0:
                    errors.append(f"{flows[key]}: negative fct_s")

    if set(flows) - set(summaries):
        for key in sorted(set(flows) - set(summaries)):
            warnings.append(f"flow CSV without summary: {flows[key]}")

    alignment_status = validate_alignment(summary_rows, flow_rows, errors)
    return summary_rows, flow_rows, {
        "observed_summary_count": str(len(summaries)),
        "observed_flow_count": str(len(flows)),
        "error_count": str(len(errors)),
        "warning_count": str(len(warnings)),
        "same_seed_alignment_status": alignment_status,
        "status": "passed" if not errors else "failed",
        "errors": " | ".join(errors),
        "warnings": " | ".join(warnings),
    }


def validate_alignment(
    summary_rows: dict[tuple, dict[str, str]],
    flow_rows: dict[tuple, list[dict[str, str]]],
    errors: list[str],
) -> str:
    grouped: dict[tuple, dict[str, tuple]] = defaultdict(dict)
    for key in summary_rows:
        scenario, scheme, seed, load, k, thetaf, agg = key
        grouped[(scenario, seed, load, k, thetaf, agg)][scheme] = key

    aligned = True
    for group_key, scheme_keys in grouped.items():
        missing = set(SCHEMES) - set(scheme_keys)
        if missing:
            errors.append(f"missing schemes for alignment group {group_key}: {sorted(missing)}")
            aligned = False
            continue
        baseline = scheme_keys["eps-ecmp"]
        baseline_by_flow = {
            flow["flow_id"]: tuple(flow.get(field, "") for field in FLOW_SEQUENCE_FIELDS)
            for flow in flow_rows[baseline]
        }
        for scheme in ("ocs-volume", "tl-ocs"):
            other = scheme_keys[scheme]
            other_by_flow = {
                flow["flow_id"]: tuple(flow.get(field, "") for field in FLOW_SEQUENCE_FIELDS)
                for flow in flow_rows[other]
            }
            if baseline_by_flow != other_by_flow:
                errors.append(f"flow sequence alignment failed for {group_key} scheme={scheme}")
                aligned = False
    return "passed" if aligned else "failed"


def write_aggregate(processed_dir: Path, summaries: dict[tuple, dict[str, str]]) -> Path:
    grouped: dict[tuple, list[dict[str, str]]] = defaultdict(list)
    for key, row in summaries.items():
        scenario, scheme, _seed, load, k, thetaf, agg = key
        grouped[(scenario, load, scheme, k, thetaf, agg)].append(row)

    fieldnames = [
        "scenario",
        "offered_load_factor",
        "scheme",
        "k",
        "thetaF",
        "aggregatorCount",
        "seed_count",
    ]
    for metric in SUMMARY_METRICS:
        fieldnames.extend([f"{metric}_mean", f"{metric}_stddev"])

    output = processed_dir / "phase15f-r2-line-summary-aggregate.csv"
    with output.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for group_key in sorted(grouped):
            scenario, load, scheme, k, thetaf, agg = group_key
            rows = grouped[group_key]
            out: dict[str, object] = {
                "scenario": scenario,
                "offered_load_factor": load,
                "scheme": scheme,
                "k": k,
                "thetaF": thetaf,
                "aggregatorCount": agg,
                "seed_count": len(rows),
            }
            for metric in SUMMARY_METRICS:
                values = [to_float(row.get(metric)) for row in rows]
                out[f"{metric}_mean"] = mean(values)
                out[f"{metric}_stddev"] = sample_std(values)
            writer.writerow({field: fmt(out.get(field, "")) for field in fieldnames})
    return output


def write_quality(processed_dir: Path, quality: dict[str, str]) -> Path:
    output = processed_dir / "phase15f-r2-line-quality-report.csv"
    fieldnames = [
        "status",
        "observed_summary_count",
        "observed_flow_count",
        "error_count",
        "warning_count",
        "same_seed_alignment_status",
        "errors",
        "warnings",
    ]
    with output.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerow({field: quality.get(field, "") for field in fieldnames})
    return output


def main() -> int:
    args = parse_args()
    args.processed_dir.mkdir(parents=True, exist_ok=True)
    summary_paths, flow_paths = discover(args.raw_dir)
    summaries, _flows, quality = validate(summary_paths, flow_paths)
    aggregate = write_aggregate(args.processed_dir, summaries)
    quality_path = write_quality(args.processed_dir, quality)
    print(f"Wrote {aggregate}")
    print(f"Wrote {quality_path}")
    if quality["status"] != "passed":
        print(quality["errors"])
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
