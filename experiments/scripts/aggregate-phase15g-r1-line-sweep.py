#!/usr/bin/env python3
"""Aggregate Phase 15G-R1 metric and scheduling diagnostics."""

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
    r"^phase15g-r1-(?P<scenario>.+)-(?P<scheme>eps-ecmp|ocs-volume|tl-ocs)-"
    r"seed(?P<seed>\d+)-load(?P<load>[0-9]+p[0-9]+)-"
    r"k(?P<k>\d+)-thetaF(?P<thetaf>\d+)-agg(?P<agg>\d+)"
    r"(?P<kind>-flows|-scheduling)?\.csv$"
)

FLOW_SEQUENCE_FIELDS = (
    "source_tor",
    "source_server",
    "destination_tor",
    "destination_server",
    "size_bytes",
    "start_time_s",
)

SUMMARY_METRICS = (
    "total_flows",
    "completed_flows",
    "incomplete_flows",
    "received_bytes",
    "avg_received_throughput_bps",
    "avg_fct_s",
    "p95_fct_s",
    "ocs_assigned_flows",
    "ocs_byte_hit_rate",
    "eps_avg_link_utilization",
    "eps_max_link_utilization",
    "avg_active_edge_count",
)

DERIVED_METRICS = (
    "total_sent_bytes",
    "actual_mean_interarrival_s",
    "completion_ratio",
    "ocs_assigned_bytes",
    "selected_edge_jaccard_mean",
    "selected_edge_jaccard_min",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raw-dir", default="results/raw", type=Path)
    parser.add_argument("--processed-dir", default="results/processed", type=Path)
    return parser.parse_args()


def parse_name(path: Path) -> dict[str, object]:
    match = RUN_RE.match(path.name)
    if not match:
        raise ValueError(f"Unexpected Phase 15G-R1 file name: {path}")
    groups = match.groupdict()
    return {
        "scenario": groups["scenario"],
        "scheme": groups["scheme"],
        "seed": int(groups["seed"]),
        "offered_load_factor": float(groups["load"].replace("p", ".")),
        "k": int(groups["k"]),
        "thetaF": int(groups["thetaf"]),
        "aggregatorCount": int(groups["agg"]),
        "kind": groups["kind"] or "summary",
    }


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as handle:
        return list(csv.DictReader(handle))


def to_float(value: str | None) -> float:
    if value in (None, ""):
        return float("nan")
    try:
        return float(value)
    except ValueError:
        return float("nan")


def fmt(value: object) -> str:
    if isinstance(value, float):
        return "" if math.isnan(value) else f"{value:.12g}"
    return str(value)


def mean(values: list[float]) -> float:
    clean = [value for value in values if not math.isnan(value)]
    return statistics.fmean(clean) if clean else float("nan")


def sample_std(values: list[float]) -> float:
    clean = [value for value in values if not math.isnan(value)]
    return statistics.stdev(clean) if len(clean) >= 2 else float("nan")


def nearest_rank(sorted_values: list[float], percentile: float) -> float:
    rank = max(1, math.ceil(percentile * len(sorted_values)))
    return sorted_values[rank - 1]


def key_from(parsed: dict[str, object]) -> tuple:
    return (
        parsed["scenario"],
        parsed["scheme"],
        parsed["seed"],
        parsed["offered_load_factor"],
        parsed["k"],
        parsed["thetaF"],
        parsed["aggregatorCount"],
    )


def discover(raw_dir: Path) -> tuple[dict[tuple, Path], dict[tuple, Path], dict[tuple, Path]]:
    summaries: dict[tuple, Path] = {}
    flows: dict[tuple, Path] = {}
    diagnostics: dict[tuple, Path] = {}
    for path in sorted(raw_dir.glob("phase15g-r1-*.csv")):
        parsed = parse_name(path)
        key = key_from(parsed)
        if parsed["kind"] == "-flows":
            flows[key] = path
        elif parsed["kind"] == "-scheduling":
            diagnostics[key] = path
        else:
            summaries[key] = path
    return summaries, flows, diagnostics


def summarize_flows(rows: list[dict[str, str]], stop_time_s: float) -> dict[str, float]:
    sizes = [to_float(row.get("size_bytes")) for row in rows]
    starts = sorted(to_float(row.get("start_time_s")) for row in rows)
    completed_fcts = [
        to_float(row.get("fct_s"))
        for row in rows
        if row.get("completed") == "true" and not math.isnan(to_float(row.get("fct_s")))
    ]
    completed_fcts.sort()
    received = sum(to_float(row.get("received_bytes")) for row in rows)
    ocs_bytes = sum(to_float(row.get("size_bytes")) for row in rows if row.get("path_type") == "ocs")
    mean_interarrival = float("nan")
    if len(starts) >= 2:
        mean_interarrival = statistics.fmean(
            starts[index] - starts[index - 1] for index in range(1, len(starts))
        )
    return {
        "total_sent_bytes": sum(sizes),
        "actual_mean_interarrival_s": mean_interarrival,
        "recalc_avg_received_throughput_bps": received * 8.0 / stop_time_s
        if stop_time_s > 0
        else float("nan"),
        "recalc_avg_fct_s": statistics.fmean(completed_fcts)
        if completed_fcts
        else float("nan"),
        "recalc_p95_fct_s": nearest_rank(completed_fcts, 0.95)
        if completed_fcts
        else float("nan"),
        "completion_ratio": len(completed_fcts) / len(rows) if rows else float("nan"),
        "ocs_assigned_bytes": ocs_bytes,
    }


def summarize_diagnostics(rows: list[dict[str, str]]) -> dict[str, float]:
    jaccards = [to_float(row.get("selected_edge_jaccard")) for row in rows]
    selected = [to_float(row.get("selected_edge_count")) for row in rows]
    active = [to_float(row.get("active_edge_count")) for row in rows]
    clean_jaccards = [value for value in jaccards if not math.isnan(value)]
    return {
        "selected_edge_jaccard_mean": mean(jaccards),
        "selected_edge_jaccard_min": min(clean_jaccards) if clean_jaccards else float("nan"),
        "selected_edge_count_mean": mean(selected),
        "active_edge_count_mean": mean(active),
    }


def almost_equal(left: float, right: float, rel_tol: float = 1e-9, abs_tol: float = 1e-12) -> bool:
    if math.isnan(left) and math.isnan(right):
        return True
    return math.isclose(left, right, rel_tol=rel_tol, abs_tol=abs_tol)


def validate(
    summaries: dict[tuple, Path],
    flows: dict[tuple, Path],
    diagnostics: dict[tuple, Path],
) -> tuple[dict[tuple, dict[str, str]], dict[tuple, dict[str, float]], dict[tuple, list[dict[str, str]]], dict[str, str]]:
    summary_rows: dict[tuple, dict[str, str]] = {}
    derived_rows: dict[tuple, dict[str, float]] = {}
    diagnostic_rows: dict[tuple, list[dict[str, str]]] = {}
    flow_rows: dict[tuple, list[dict[str, str]]] = {}
    errors: list[str] = []
    warnings: list[str] = []

    if not summaries:
        errors.append("no phase15g-r1 summary CSV files found")

    for key, summary_path in summaries.items():
        if key not in flows:
            errors.append(f"missing flow CSV for {summary_path}")
            continue
        rows = read_csv(summary_path)
        if len(rows) != 1:
            errors.append(f"{summary_path}: expected one summary row, found {len(rows)}")
            continue
        summary = rows[0]
        flow_data = read_csv(flows[key])
        summary_rows[key] = summary
        flow_rows[key] = flow_data

        total = int(to_float(summary.get("total_flows")))
        completed = int(to_float(summary.get("completed_flows")))
        incomplete = int(to_float(summary.get("incomplete_flows")))
        if total != len(flow_data):
            errors.append(f"{flows[key]}: row count {len(flow_data)} != total_flows {total}")
        if completed + incomplete != total:
            errors.append(f"{summary_path}: completed + incomplete != total")
        if int(to_float(summary.get("installed_flows"))) != total:
            errors.append(f"{summary_path}: installed_flows != total_flows")

        stop_time_s = to_float(summary.get("stop_time_s"))
        derived = summarize_flows(flow_data, stop_time_s)
        derived_rows[key] = derived
        for summary_field, derived_field in (
            ("avg_received_throughput_bps", "recalc_avg_received_throughput_bps"),
            ("avg_fct_s", "recalc_avg_fct_s"),
            ("p95_fct_s", "recalc_p95_fct_s"),
        ):
            if not almost_equal(to_float(summary.get(summary_field)), derived[derived_field], rel_tol=1e-6):
                errors.append(f"{summary_path}: {summary_field} does not match per-flow recompute")

        if incomplete > 0:
            warnings.append(f"{summary_path}: incomplete flows present; FCT metrics use completed flows")
        if key[1] == "eps-ecmp":
            if any(row.get("path_type") != "eps" for row in flow_data):
                errors.append(f"{flows[key]}: EPS run contains non-EPS path_type")
            if int(to_float(summary.get("ocs_assigned_flows"))) != 0:
                errors.append(f"{summary_path}: EPS run has OCS assignments")
            if to_float(summary.get("ocs_byte_hit_rate")) != 0.0:
                errors.append(f"{summary_path}: EPS run has nonzero OCS byte hit rate")
        else:
            if key not in diagnostics:
                errors.append(f"missing scheduling diagnostics for {summary_path}")
            else:
                diagnostic_data = read_csv(diagnostics[key])
                diagnostic_rows[key] = diagnostic_data
                derived_rows[key].update(summarize_diagnostics(diagnostic_data))

    alignment_status = validate_alignment(flow_rows, errors)
    validate_load_mapping(summary_rows, derived_rows, warnings)
    return summary_rows, derived_rows, diagnostic_rows, {
        "status": "passed" if not errors else "failed",
        "observed_summary_count": str(len(summaries)),
        "observed_flow_count": str(len(flows)),
        "observed_scheduling_count": str(len(diagnostics)),
        "error_count": str(len(errors)),
        "warning_count": str(len(warnings)),
        "same_seed_alignment_status": alignment_status,
        "errors": " | ".join(errors),
        "warnings": " | ".join(warnings),
    }


def validate_alignment(flow_rows: dict[tuple, list[dict[str, str]]], errors: list[str]) -> str:
    grouped: dict[tuple, dict[str, tuple]] = defaultdict(dict)
    for key in flow_rows:
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
            row["flow_id"]: tuple(row.get(field, "") for field in FLOW_SEQUENCE_FIELDS)
            for row in flow_rows[baseline]
        }
        for scheme in ("ocs-volume", "tl-ocs"):
            other = scheme_keys[scheme]
            other_by_flow = {
                row["flow_id"]: tuple(row.get(field, "") for field in FLOW_SEQUENCE_FIELDS)
                for row in flow_rows[other]
            }
            if baseline_by_flow != other_by_flow:
                errors.append(f"flow sequence alignment failed for {group_key} scheme={scheme}")
                aligned = False
    return "passed" if aligned else "failed"


def validate_load_mapping(
    summary_rows: dict[tuple, dict[str, str]],
    derived_rows: dict[tuple, dict[str, float]],
    warnings: list[str],
) -> None:
    grouped: dict[tuple, list[tuple[float, dict[str, str], dict[str, float]]]] = defaultdict(list)
    for key, row in summary_rows.items():
        scenario, scheme, seed, _load, k, thetaf, agg = key
        grouped[(scenario, scheme, seed, k, thetaf, agg)].append((key[3], row, derived_rows[key]))

    for group_key, entries in grouped.items():
        if len(entries) < 2:
            continue
        entries.sort(key=lambda item: item[0])
        first = entries[0]
        last = entries[-1]
        first_bytes = first[2]["total_sent_bytes"]
        last_bytes = last[2]["total_sent_bytes"]
        first_throughput = to_float(first[1].get("avg_received_throughput_bps"))
        last_throughput = to_float(last[1].get("avg_received_throughput_bps"))
        if first_bytes > 0 and last_bytes / first_bytes < 1.2:
            warnings.append(f"{group_key}: total_sent_bytes changed by <20% across loads")
        if first_throughput > 0 and last_throughput / first_throughput < 1.2:
            warnings.append(f"{group_key}: received throughput changed by <20% across loads")
        for previous, current in zip(entries, entries[1:]):
            previous_load, previous_row, previous_derived = previous
            current_load, current_row, current_derived = current
            if current_derived["total_sent_bytes"] <= previous_derived["total_sent_bytes"]:
                warnings.append(
                    f"{group_key}: total_sent_bytes did not increase from load "
                    f"{previous_load} to {current_load}"
                )
            if to_float(current_row.get("avg_received_throughput_bps")) <= to_float(
                previous_row.get("avg_received_throughput_bps")
            ):
                warnings.append(
                    f"{group_key}: received throughput did not increase from load "
                    f"{previous_load} to {current_load}"
                )


def write_aggregate(
    processed_dir: Path,
    summaries: dict[tuple, dict[str, str]],
    derived: dict[tuple, dict[str, float]],
) -> Path:
    grouped: dict[tuple, list[tuple[dict[str, str], dict[str, float]]]] = defaultdict(list)
    for key, row in summaries.items():
        scenario, scheme, _seed, load, k, thetaf, agg = key
        grouped[(scenario, load, scheme, k, thetaf, agg)].append((row, derived[key]))

    fieldnames = [
        "scenario",
        "offered_load_factor",
        "scheme",
        "k",
        "thetaF",
        "aggregatorCount",
        "seed_count",
    ]
    for metric in SUMMARY_METRICS + DERIVED_METRICS:
        fieldnames.extend([f"{metric}_mean", f"{metric}_stddev"])

    output = processed_dir / "phase15g-r1-line-summary-aggregate.csv"
    with output.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for group_key in sorted(grouped):
            scenario, load, scheme, k, thetaf, agg = group_key
            entries = grouped[group_key]
            out: dict[str, object] = {
                "scenario": scenario,
                "offered_load_factor": load,
                "scheme": scheme,
                "k": k,
                "thetaF": thetaf,
                "aggregatorCount": agg,
                "seed_count": len(entries),
            }
            for metric in SUMMARY_METRICS:
                values = [to_float(row.get(metric)) for row, _derived in entries]
                out[f"{metric}_mean"] = mean(values)
                out[f"{metric}_stddev"] = sample_std(values)
            for metric in DERIVED_METRICS:
                values = [_derived.get(metric, float("nan")) for _row, _derived in entries]
                out[f"{metric}_mean"] = mean(values)
                out[f"{metric}_stddev"] = sample_std(values)
            writer.writerow({field: fmt(out.get(field, "")) for field in fieldnames})
    return output


def write_scheduling_aggregate(
    processed_dir: Path,
    diagnostics: dict[tuple, list[dict[str, str]]],
) -> Path:
    fieldnames = [
        "scenario",
        "scheme",
        "seed",
        "offered_load_factor",
        "round_count",
        "selected_edge_jaccard_mean",
        "selected_edge_jaccard_min",
        "first_raw_a_top_edges",
        "first_tl_g_top_edges",
        "first_volume_selected_edges",
        "first_tl_ocs_selected_edges",
    ]
    output = processed_dir / "phase15g-r1-scheduling-diagnostics-aggregate.csv"
    with output.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for key, rows in sorted(diagnostics.items()):
            scenario, scheme, seed, load, _k, _thetaf, _agg = key
            jaccards = [to_float(row.get("selected_edge_jaccard")) for row in rows]
            clean = [value for value in jaccards if not math.isnan(value)]
            first = rows[0] if rows else {}
            writer.writerow(
                {
                    "scenario": scenario,
                    "scheme": scheme,
                    "seed": seed,
                    "offered_load_factor": fmt(load),
                    "round_count": len(rows),
                    "selected_edge_jaccard_mean": fmt(mean(jaccards)),
                    "selected_edge_jaccard_min": fmt(min(clean) if clean else float("nan")),
                    "first_raw_a_top_edges": first.get("raw_a_top_edges", ""),
                    "first_tl_g_top_edges": first.get("tl_g_top_edges", ""),
                    "first_volume_selected_edges": first.get("volume_selected_edges", ""),
                    "first_tl_ocs_selected_edges": first.get("tl_ocs_selected_edges", ""),
                }
            )
    return output


def write_quality(processed_dir: Path, quality: dict[str, str]) -> Path:
    output = processed_dir / "phase15g-r1-line-quality-report.csv"
    fieldnames = [
        "status",
        "observed_summary_count",
        "observed_flow_count",
        "observed_scheduling_count",
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
    summaries, flows, diagnostics = discover(args.raw_dir)
    summary_rows, derived, diagnostic_rows, quality = validate(summaries, flows, diagnostics)
    aggregate = write_aggregate(args.processed_dir, summary_rows, derived)
    scheduling = write_scheduling_aggregate(args.processed_dir, diagnostic_rows)
    quality_path = write_quality(args.processed_dir, quality)
    print(f"Wrote {aggregate}")
    print(f"Wrote {scheduling}")
    print(f"Wrote {quality_path}")
    if quality["status"] != "passed":
        print(quality["errors"])
        return 1
    if quality["warnings"]:
        print(quality["warnings"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
