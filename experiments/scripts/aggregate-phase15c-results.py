#!/usr/bin/env python3
"""Aggregate and quality-check Phase 15C TL-OCS raw CSV results."""

from __future__ import annotations

import argparse
import csv
import math
import re
import statistics
from collections import defaultdict
from pathlib import Path


SCHEMES = ("eps-ecmp", "ocs-volume", "tl-ocs")
GROUPS = (
    "uniform-main",
    "community-main",
    "aggregation-main",
    "aggregation-thetaf",
    "community-k2",
    "aggregation-k2",
    "aggregation-agg2",
    "aggregation-agg2-thetaf",
)

RUN_RE = re.compile(
    r"^phase15c-(?P<group>.+)-(?P<scheme>eps-ecmp|ocs-volume|tl-ocs)-"
    r"seed(?P<seed>\d+)-k(?P<k>\d+)-thetaF(?P<thetaf>\d+)-agg(?P<agg>\d+)"
    r"(?P<flows>-flows)?\.csv$"
)

AGGREGATE_METRICS = (
    "completed_flows",
    "total_flows",
    "received_bytes",
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
    "scheduling_round_count",
    "non_empty_scheduling_rounds",
    "avg_selected_edge_count",
    "max_selected_edge_count",
    "avg_active_edge_count",
    "max_active_edge_count",
    "total_active_lightpath_seconds",
    "community_internal_selected_edge_ratio",
)

COMPARISON_METRICS = (
    "avg_received_throughput_bps",
    "avg_fct_s",
    "p90_fct_s",
    "p95_fct_s",
    "ocs_assigned_flows",
    "ocs_flow_hit_rate",
    "ocs_byte_hit_rate",
    "eps_avg_link_utilization",
    "ocs_avg_link_utilization",
    "avg_selected_edge_count",
    "avg_active_edge_count",
    "community_internal_selected_edge_ratio",
)

DIRECTION_HINTS = {
    "avg_received_throughput_bps": "higher_is_better",
    "avg_fct_s": "lower_is_better",
    "p90_fct_s": "lower_is_better",
    "p95_fct_s": "lower_is_better",
    "ocs_assigned_flows": "descriptive_only",
    "ocs_flow_hit_rate": "descriptive_only",
    "ocs_byte_hit_rate": "descriptive_only",
    "eps_avg_link_utilization": "descriptive_only",
    "ocs_avg_link_utilization": "descriptive_only",
    "avg_selected_edge_count": "descriptive_only",
    "avg_active_edge_count": "descriptive_only",
    "community_internal_selected_edge_ratio": "descriptive_only",
}

COMPARISONS = (
    ("eps-ecmp", "tl-ocs"),
    ("ocs-volume", "tl-ocs"),
    ("eps-ecmp", "ocs-volume"),
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
    parser.add_argument("--manifest", default="docs/phase15c-raw-data-manifest.md", type=Path)
    parser.add_argument("--report", default="docs/phase15d-aggregation-report.md", type=Path)
    return parser.parse_args()


def parse_run_name(path: Path) -> dict[str, object]:
    match = RUN_RE.match(path.name)
    if not match:
        raise ValueError(f"Unexpected Phase 15C file name: {path}")
    values = match.groupdict()
    return {
        "group": values["group"],
        "scheme": values["scheme"],
        "seed": int(values["seed"]),
        "k": int(values["k"]),
        "thetaF": int(values["thetaf"]),
        "aggregatorCount": int(values["agg"]),
        "is_flow": bool(values["flows"]),
    }


def read_single_row_csv(path: Path) -> tuple[list[str], dict[str, str]]:
    with path.open(newline="") as handle:
        rows = list(csv.DictReader(handle))
    if len(rows) != 1:
        raise ValueError(f"Expected exactly one summary row in {path}, found {len(rows)}")
    return list(rows[0].keys()), rows[0]


def read_flow_csv(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    with path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        return list(reader.fieldnames or []), list(reader)


def numeric(value: str | None) -> float:
    if value is None or value == "":
        return float("nan")
    try:
        return float(value)
    except ValueError:
        return float("nan")


def mean(values: list[float]) -> float:
    clean = [value for value in values if not math.isnan(value)]
    if not clean:
        return float("nan")
    return statistics.fmean(clean)


def sample_std(values: list[float]) -> float:
    clean = [value for value in values if not math.isnan(value)]
    if len(clean) < 2:
        return float("nan")
    return statistics.stdev(clean)


def fmt(value: object) -> str:
    if isinstance(value, float):
        if math.isnan(value):
            return ""
        return f"{value:.12g}"
    return str(value)


def load_manifest(path: Path) -> dict[tuple[str, str, int, int, int, int], dict[str, str]]:
    if not path.exists():
        raise FileNotFoundError(path)
    lines = path.read_text().splitlines()
    csv_lines: list[str] = []
    inside = False
    for line in lines:
        if line.strip() == "```csv":
            inside = True
            continue
        if inside and line.strip() == "```":
            break
        if inside:
            csv_lines.append(line)
    if not csv_lines:
        raise ValueError(f"No CSV run inventory found in {path}")
    reader = csv.DictReader(csv_lines)
    manifest: dict[tuple[str, str, int, int, int, int], dict[str, str]] = {}
    for row in reader:
        key = (
            row["group"],
            row["scheme"],
            int(row["seed"]),
            int(row["k"]),
            int(row["thetaF"]),
            int(row["aggregatorCount"]),
        )
        manifest[key] = row
    return manifest


def discover_raw_files(raw_dir: Path) -> tuple[dict[tuple, Path], dict[tuple, Path]]:
    summary: dict[tuple, Path] = {}
    flows: dict[tuple, Path] = {}
    for path in sorted(raw_dir.glob("phase15c-*.csv")):
        parsed = parse_run_name(path)
        key = (
            parsed["group"],
            parsed["scheme"],
            parsed["seed"],
            parsed["k"],
            parsed["thetaF"],
            parsed["aggregatorCount"],
        )
        if parsed["is_flow"]:
            flows[key] = path
        else:
            summary[key] = path
    return summary, flows


def validate_manifest(
    manifest: dict[tuple, dict[str, str]],
    summary_paths: dict[tuple, Path],
    flow_paths: dict[tuple, Path],
) -> list[str]:
    errors: list[str] = []
    manifest_keys = set(manifest)
    summary_keys = set(summary_paths)
    flow_keys = set(flow_paths)
    for key in sorted(manifest_keys - summary_keys):
        errors.append(f"missing summary for manifest run {key}")
    for key in sorted(manifest_keys - flow_keys):
        errors.append(f"missing flow CSV for manifest run {key}")
    for key in sorted(summary_keys - manifest_keys):
        errors.append(f"extra summary not listed in manifest {summary_paths[key]}")
    for key in sorted(flow_keys - manifest_keys):
        errors.append(f"extra flow CSV not listed in manifest {flow_paths[key]}")
    for key, row in manifest.items():
        if row.get("status") != "passed":
            errors.append(f"manifest run not passed: {key} status={row.get('status')}")
        if key in summary_paths and row.get("summary_csv") != str(summary_paths[key]):
            errors.append(f"summary path mismatch for {key}")
        if key in flow_paths and row.get("flows_csv") != str(flow_paths[key]):
            errors.append(f"flow path mismatch for {key}")
    return errors


def validate_raw_data(
    summary_paths: dict[tuple, Path],
    flow_paths: dict[tuple, Path],
) -> tuple[dict[tuple, dict[str, str]], dict[tuple, list[dict[str, str]]], dict[str, object], list[str]]:
    summaries: dict[tuple, dict[str, str]] = {}
    flows: dict[tuple, list[dict[str, str]]] = {}
    errors: list[str] = []
    statuses = {
        "eps_only_zero_ocs_status": "passed",
        "path_type_domain_status": "passed",
        "fct_nonnegative_status": "passed",
        "hit_rate_range_status": "passed",
        "utilization_range_status": "passed",
    }

    for key, path in sorted(summary_paths.items()):
        header, row = read_single_row_csv(path)
        if len(header) != len(row):
            errors.append(f"summary header/value mismatch in {path}")
        summaries[key] = row
        total = int(numeric(row.get("total_flows")))
        installed = int(numeric(row.get("installed_flows")))
        completed = int(numeric(row.get("completed_flows")))
        incomplete = int(numeric(row.get("incomplete_flows")))
        if total != installed:
            errors.append(f"total_flows != installed_flows in {path}")
        if completed + incomplete != total:
            errors.append(f"completed+incomplete != total in {path}")
        if numeric(row.get("received_bytes")) <= 0:
            errors.append(f"received_bytes <= 0 in {path}")
        if numeric(row.get("avg_received_throughput_bps")) <= 0:
            errors.append(f"throughput <= 0 in {path}")
        for field in ("ocs_flow_hit_rate", "ocs_byte_hit_rate"):
            value = numeric(row.get(field))
            if math.isnan(value) or value < 0 or value > 1:
                statuses["hit_rate_range_status"] = "failed"
                errors.append(f"{field} out of range in {path}")
        for field in (
            "eps_avg_link_utilization",
            "eps_max_link_utilization",
            "ocs_avg_link_utilization",
            "ocs_max_link_utilization",
        ):
            value = numeric(row.get(field))
            if math.isnan(value) or value < 0:
                statuses["utilization_range_status"] = "failed"
                errors.append(f"{field} invalid in {path}")
        if key[1] == "eps-ecmp":
            eps_zero = (
                numeric(row.get("ocs_assigned_flows")) == 0
                and numeric(row.get("ocs_flow_hit_rate")) == 0
                and numeric(row.get("ocs_byte_hit_rate")) == 0
                and numeric(row.get("ocs_avg_link_utilization")) == 0
                and numeric(row.get("ocs_max_link_utilization")) == 0
            )
            if not eps_zero:
                statuses["eps_only_zero_ocs_status"] = "failed"
                errors.append(f"EPS-only OCS fields nonzero in {path}")
        bound = int(key[3]) * int(numeric(row.get("num_tors"))) / 2
        for field in ("avg_selected_edge_count", "max_selected_edge_count", "avg_active_edge_count", "max_active_edge_count"):
            if numeric(row.get(field)) > bound:
                errors.append(f"{field} exceeds port bound in {path}")

    for key, path in sorted(flow_paths.items()):
        header, rows = read_flow_csv(path)
        flows[key] = rows
        expected_rows = int(numeric(summaries[key].get("total_flows"))) if key in summaries else None
        if expected_rows is not None and len(rows) != expected_rows:
            errors.append(f"flow row count mismatch in {path}")
        for row in rows:
            if row.get("path_type") not in {"eps", "ocs"}:
                statuses["path_type_domain_status"] = "failed"
                errors.append(f"invalid path_type in {path}: {row.get('path_type')}")
            completed = row.get("completed", "").lower() == "true"
            if completed:
                if row.get("completion_time_s", "") == "" or row.get("fct_s", "") == "":
                    errors.append(f"completed flow missing completion/FCT in {path}")
                if numeric(row.get("fct_s")) < 0:
                    statuses["fct_nonnegative_status"] = "failed"
                    errors.append(f"negative FCT in {path}")
                if int(numeric(row.get("received_bytes"))) != int(numeric(row.get("size_bytes"))):
                    errors.append(f"completed received_bytes != size_bytes in {path}")

    incomplete_run_count = sum(1 for row in summaries.values() if numeric(row.get("incomplete_flows")) > 0)
    quality = {
        "expected_run_count": 72,
        "observed_summary_count": len(summary_paths),
        "observed_flow_count": len(flow_paths),
        "missing_summary_count": max(0, 72 - len(summary_paths)),
        "missing_flow_count": max(0, 72 - len(flow_paths)),
        "failed_run_count": 0,
        "incomplete_run_count": incomplete_run_count,
        **statuses,
    }
    return summaries, flows, quality, errors


def check_same_seed_alignment(flows: dict[tuple, list[dict[str, str]]]) -> tuple[str, list[str]]:
    errors: list[str] = []
    by_point: dict[tuple, dict[str, list[dict[str, str]]]] = defaultdict(dict)
    for key, rows in flows.items():
        group, scheme, seed, k, thetaf, agg = key
        by_point[(group, seed, k, thetaf, agg)][scheme] = rows
    for point, scheme_rows in sorted(by_point.items()):
        missing = set(SCHEMES) - set(scheme_rows)
        if missing:
            errors.append(f"missing schemes for alignment point {point}: {sorted(missing)}")
            continue
        reference_scheme = "eps-ecmp"
        reference = {row["flow_id"]: row for row in scheme_rows[reference_scheme]}
        for scheme in ("ocs-volume", "tl-ocs"):
            candidate = {row["flow_id"]: row for row in scheme_rows[scheme]}
            if set(reference) != set(candidate):
                errors.append(f"flow_id set mismatch at {point} for {scheme}")
                continue
            for flow_id, ref_row in reference.items():
                cand_row = candidate[flow_id]
                for field in FLOW_SEQUENCE_FIELDS:
                    if ref_row.get(field) != cand_row.get(field):
                        errors.append(
                            f"alignment mismatch {point} scheme={scheme} flow_id={flow_id} "
                            f"field={field} ref={ref_row.get(field)} got={cand_row.get(field)}"
                        )
                        break
    return ("passed" if not errors else "failed"), errors


def aggregate_summaries(summaries: dict[tuple, dict[str, str]]) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for group in GROUPS:
        for scheme in SCHEMES:
            matching = [row for key, row in summaries.items() if key[0] == group and key[1] == scheme]
            if not matching:
                continue
            out: dict[str, object] = {"group": group, "scheme": scheme, "seed_count": len(matching)}
            out["incomplete_flows_sum"] = sum(numeric(row.get("incomplete_flows")) for row in matching)
            for metric in AGGREGATE_METRICS:
                values = [numeric(row.get(metric)) for row in matching]
                out[f"{metric}_mean"] = mean(values)
                out[f"{metric}_stddev"] = sample_std(values)
            rows.append(out)
    return rows


def comparison_rows(aggregate_rows: list[dict[str, object]]) -> list[dict[str, object]]:
    by_group_scheme = {(row["group"], row["scheme"]): row for row in aggregate_rows}
    rows: list[dict[str, object]] = []
    for group in GROUPS:
        for baseline, target in COMPARISONS:
            base_row = by_group_scheme[(group, baseline)]
            target_row = by_group_scheme[(group, target)]
            for metric in COMPARISON_METRICS:
                baseline_mean = float(base_row[f"{metric}_mean"])
                target_mean = float(target_row[f"{metric}_mean"])
                absolute_delta = target_mean - baseline_mean
                if baseline_mean == 0 or math.isnan(baseline_mean):
                    relative_delta = float("nan")
                else:
                    relative_delta = absolute_delta / abs(baseline_mean) * 100
                rows.append(
                    {
                        "group": group,
                        "baseline_scheme": baseline,
                        "target_scheme": target,
                        "metric": metric,
                        "baseline_mean": baseline_mean,
                        "target_mean": target_mean,
                        "absolute_delta": absolute_delta,
                        "relative_delta_percent": relative_delta,
                        "direction_hint": DIRECTION_HINTS[metric],
                    }
                )
    return rows


def write_csv(path: Path, rows: list[dict[str, object]], fieldnames: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow({field: fmt(row.get(field, "")) for field in fieldnames})


def write_report(
    path: Path,
    aggregate_rows: list[dict[str, object]],
    comparison: list[dict[str, object]],
    quality: dict[str, object],
    alignment_status: str,
) -> None:
    by_group = defaultdict(list)
    for row in aggregate_rows:
        by_group[row["group"]].append(row)

    lines: list[str] = [
        "# Phase 15D Aggregation Report",
        "",
        "This report aggregates Phase 15C raw CSV files for data-quality and statistics preparation. It is not paper text and does not state paper conclusions.",
        "",
        "## Inputs",
        "",
        "- Source raw files: `results/raw/phase15c-*.csv`",
        "- Summary CSV count: 72",
        "- Per-flow CSV count: 72",
        "- Experiment commit for raw data: `c2e0c22 Add V5 alignment and calibration reports`",
        "- Current documentation commit before Phase 15D: `3bf86be Record V5 phase15c formal raw batch`",
        "",
        "## Outputs",
        "",
        "- `results/processed/phase15c-summary-aggregate.csv`",
        "- `results/processed/phase15c-scheme-comparison.csv`",
        "- `results/processed/phase15c-quality-report.csv`",
        "",
        "The processed CSV files are generated artifacts and are not intended to be committed by default.",
        "",
        "## Quality Review",
        "",
        f"- Expected runs: `{quality['expected_run_count']}`",
        f"- Observed summary files: `{quality['observed_summary_count']}`",
        f"- Observed per-flow files: `{quality['observed_flow_count']}`",
        f"- Missing summary files: `{quality['missing_summary_count']}`",
        f"- Missing per-flow files: `{quality['missing_flow_count']}`",
        f"- Failed runs: `{quality['failed_run_count']}`",
        f"- Runs with incomplete flows: `{quality['incomplete_run_count']}`",
        f"- Same-seed flow sequence alignment: `{alignment_status}`",
        f"- EPS-only zero OCS checks: `{quality['eps_only_zero_ocs_status']}`",
        f"- Path type domain checks: `{quality['path_type_domain_status']}`",
        f"- FCT nonnegative checks: `{quality['fct_nonnegative_status']}`",
        f"- Hit-rate range checks: `{quality['hit_rate_range_status']}`",
        f"- Utilization range checks: `{quality['utilization_range_status']}`",
        "",
        "No failed, missing, or incomplete-flow runs were found in the Phase 15C raw set.",
        "",
        "## Group Scheme Aggregate Summary",
        "",
    ]

    for group in GROUPS:
        lines += [f"### {group}", ""]
        for row in sorted(by_group[group], key=lambda item: SCHEMES.index(str(item["scheme"]))):
            lines.append(
                "- `{scheme}`: seeds `{seed_count}`, completed `{completed:.12g}/{total:.12g}`, "
                "throughput `{throughput:.12g}`, avg/p90/p95 FCT `{avg_fct:.12g}` / `{p90:.12g}` / `{p95:.12g}`, "
                "OCS assigned `{ocs_assigned:.12g}`, EPS fallback `{eps_fallback:.12g}`, OCS hit `{ocs_hit:.12g}`, "
                "OCS byte hit `{ocs_byte:.12g}`, EPS avg/max util `{eps_avg:.12g}` / `{eps_max:.12g}`, "
                "OCS avg/max util `{ocs_avg:.12g}` / `{ocs_max:.12g}`, non-empty rounds `{non_empty:.12g}`, "
                "avg active edges `{avg_active:.12g}`, active lightpath seconds `{active_time:.12g}`, community ratio `{community_ratio:.12g}`.".format(
                    scheme=row["scheme"],
                    seed_count=row["seed_count"],
                    completed=float(row["completed_flows_mean"]),
                    total=float(row["total_flows_mean"]),
                    throughput=float(row["avg_received_throughput_bps_mean"]),
                    avg_fct=float(row["avg_fct_s_mean"]),
                    p90=float(row["p90_fct_s_mean"]),
                    p95=float(row["p95_fct_s_mean"]),
                    ocs_assigned=float(row["ocs_assigned_flows_mean"]),
                    eps_fallback=float(row["eps_fallback_flows_mean"]),
                    ocs_hit=float(row["ocs_flow_hit_rate_mean"]),
                    ocs_byte=float(row["ocs_byte_hit_rate_mean"]),
                    eps_avg=float(row["eps_avg_link_utilization_mean"]),
                    eps_max=float(row["eps_max_link_utilization_mean"]),
                    ocs_avg=float(row["ocs_avg_link_utilization_mean"]),
                    ocs_max=float(row["ocs_max_link_utilization_mean"]),
                    non_empty=float(row["non_empty_scheduling_rounds_mean"]),
                    avg_active=float(row["avg_active_edge_count_mean"]),
                    active_time=float(row["total_active_lightpath_seconds_mean"]),
                    community_ratio=float(row["community_internal_selected_edge_ratio_mean"]),
                )
            )
        lines.append("")

    visible_differences: list[str] = []
    weak_differences: list[str] = []
    for group in GROUPS:
        relevant = [row for row in comparison if row["group"] == group and row["baseline_scheme"] == "ocs-volume" and row["target_scheme"] == "tl-ocs"]
        changed = [
            row for row in relevant
            if row["metric"] in {"avg_fct_s", "ocs_assigned_flows", "ocs_flow_hit_rate", "ocs_byte_hit_rate", "ocs_avg_link_utilization", "avg_selected_edge_count", "avg_active_edge_count"}
            and abs(float(row["absolute_delta"])) > 1e-12
        ]
        if changed:
            visible_differences.append(group)
        else:
            weak_differences.append(group)

    lines += [
        "## Difference Readiness Notes",
        "",
        "- Groups with observable TL-OCS versus OCS-Volume aggregate deltas: " + ", ".join(f"`{g}`" for g in visible_differences) + ".",
        "- Groups with weak or no TL-OCS versus OCS-Volume aggregate deltas: " + ", ".join(f"`{g}`" for g in weak_differences) + ".",
        "- The comparison CSV records absolute and relative deltas with neutral `direction_hint` values only. It intentionally avoids significance or paper-conclusion language.",
        "",
        "## Phase 15E Readiness",
        "",
        "- Data quality and same-seed alignment are suitable for Phase 15E plotting preparation and visual inspection.",
        "- Phase 15E should consume the processed CSVs generated by this script and keep raw CSVs immutable.",
        "- Any statistical claims should wait for an explicit statistics phase; this report only prepares aggregate data.",
    ]
    path.write_text("\n".join(lines) + "\n")


def main() -> int:
    args = parse_args()
    manifest = load_manifest(args.manifest)
    summary_paths, flow_paths = discover_raw_files(args.raw_dir)

    manifest_errors = validate_manifest(manifest, summary_paths, flow_paths)
    summaries, flows, quality, raw_errors = validate_raw_data(summary_paths, flow_paths)
    alignment_status, alignment_errors = check_same_seed_alignment(flows)
    quality["same_seed_alignment_status"] = alignment_status

    errors = manifest_errors + raw_errors + alignment_errors
    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 1

    aggregate_rows = aggregate_summaries(summaries)
    comparison = comparison_rows(aggregate_rows)

    aggregate_fields = ["group", "scheme", "seed_count", "incomplete_flows_sum"]
    for metric in AGGREGATE_METRICS:
        aggregate_fields.extend([f"{metric}_mean", f"{metric}_stddev"])
    write_csv(args.processed_dir / "phase15c-summary-aggregate.csv", aggregate_rows, aggregate_fields)

    comparison_fields = [
        "group",
        "baseline_scheme",
        "target_scheme",
        "metric",
        "baseline_mean",
        "target_mean",
        "absolute_delta",
        "relative_delta_percent",
        "direction_hint",
    ]
    write_csv(args.processed_dir / "phase15c-scheme-comparison.csv", comparison, comparison_fields)

    quality_fields = [
        "expected_run_count",
        "observed_summary_count",
        "observed_flow_count",
        "missing_summary_count",
        "missing_flow_count",
        "failed_run_count",
        "incomplete_run_count",
        "same_seed_alignment_status",
        "eps_only_zero_ocs_status",
        "path_type_domain_status",
        "fct_nonnegative_status",
        "hit_rate_range_status",
        "utilization_range_status",
    ]
    write_csv(args.processed_dir / "phase15c-quality-report.csv", [quality], quality_fields)

    write_report(args.report, aggregate_rows, comparison, quality, alignment_status)

    print("PHASE15C_AGGREGATION_PASS")
    print(f"summary_rows={len(aggregate_rows)}")
    print(f"comparison_rows={len(comparison)}")
    print(f"quality_report={args.processed_dir / 'phase15c-quality-report.csv'}")
    print(f"report={args.report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
