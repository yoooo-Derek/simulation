#!/usr/bin/env python3
"""Aggregate Phase 15H-R4 matrix-replay validation outputs."""

import csv
import math
import re
from collections import defaultdict
from pathlib import Path

RAW_DIR = Path("results/raw")
PROCESSED_DIR = Path("results/processed")
SUMMARY_OUT = PROCESSED_DIR / "phase15h-r4-mechanism-summary.csv"
QUALITY_OUT = PROCESSED_DIR / "phase15h-r4-mechanism-quality-report.csv"
SCHED_OUT = PROCESSED_DIR / "phase15h-r4-scheduling-diagnostics.csv"
CLOSENESS_OUT = PROCESSED_DIR / "phase15h-r4-oracle-closeness.csv"
AUDIT_IN = PROCESSED_DIR / "phase15h-r4-replay-matrix-audit.csv"

NAME_RE = re.compile(
    r"^phase15h-r4-(?P<scenario>.+)-"
    r"(?P<scheme>eps-ecmp|ocs-volume|tl-ocs|ocs-oracle)-"
    r"seed(?P<seed>\d+)-load(?P<load>[0-9]+p[0-9]+)$"
)

SUMMARY_FIELDS = [
    "scenario",
    "scheme",
    "seed",
    "offered_load_factor",
    "stop_time_s",
    "flow_count",
    "completed_flow_count",
    "incomplete_flow_count",
    "completion_ratio",
    "total_sent_bytes",
    "total_received_bytes",
    "actual_offered_bps",
    "actual_received_bps",
    "avg_received_throughput_bps",
    "avg_fct_s",
    "p95_fct_s",
    "eps_path_count",
    "ocs_path_count",
    "ocs_assigned_flows",
    "ocs_assigned_bytes",
    "ocs_byte_hit_rate",
    "eps_avg_link_utilization",
    "eps_max_link_utilization",
    "avg_volume_tl_jaccard",
    "avg_volume_oracle_jaccard",
    "avg_tl_oracle_jaccard",
    "avg_selected_oracle_jaccard",
    "avg_volume_future_demand_coverage",
    "avg_tl_future_demand_coverage",
    "avg_oracle_future_demand_coverage",
    "avg_historical_future_pearson",
    "avg_historical_future_topk_jaccard",
    "avg_demand_drift_ratio",
    "selected_but_unused_lightpaths",
    "volume_oracle_possible_bytes_missed",
    "tl_oracle_possible_bytes_missed",
    "avg_corr_target_matrix_d_future",
    "avg_target_d_future_matrix_error",
    "replay_audit_gate_pass",
]

SCHED_FIELDS = [
    "scenario",
    "scheme",
    "seed",
    "offered_load_factor",
    "cycle",
    "round_start_s",
    "round_end_s",
    "observed_matrix_bytes",
    "future_demand_bytes",
    "selected_edge_count",
    "active_edge_count",
    "selected_hit_flows",
    "selected_hit_bytes",
    "selected_but_unused_lightpaths",
    "selected_edge_jaccard",
    "selected_oracle_jaccard",
    "volume_oracle_jaccard",
    "tl_oracle_jaccard",
    "volume_future_demand_coverage",
    "tl_future_demand_coverage",
    "oracle_future_demand_coverage",
    "historical_future_pearson",
    "historical_future_topk_jaccard",
    "demand_drift_ratio",
    "volume_oracle_possible_bytes_missed",
    "tl_oracle_possible_bytes_missed",
    "volume_selected_edges",
    "tl_ocs_selected_edges",
    "oracle_selected_edges",
    "raw_a_top_edges",
    "tl_g_top_edges",
    "future_demand_top_edges",
]

CLOSENESS_FIELDS = [
    "scenario",
    "seed",
    "offered_load_factor",
    "scheme",
    "avg_fct_s",
    "oracle_avg_fct_s",
    "avg_fct_ratio_vs_oracle",
    "p95_fct_s",
    "oracle_p95_fct_s",
    "p95_fct_ratio_vs_oracle",
    "completion_ratio",
    "oracle_completion_ratio",
    "completion_ratio_gap_vs_oracle",
    "avg_received_throughput_bps",
    "oracle_avg_received_throughput_bps",
    "throughput_ratio_vs_oracle",
    "avg_selected_oracle_jaccard",
    "avg_future_demand_coverage",
    "oracle_future_demand_coverage",
    "oracle_possible_bytes_missed",
]

SCHED_SUMMARY_DEFAULTS = [
    "avg_volume_tl_jaccard",
    "avg_volume_oracle_jaccard",
    "avg_tl_oracle_jaccard",
    "avg_selected_oracle_jaccard",
    "avg_volume_future_demand_coverage",
    "avg_tl_future_demand_coverage",
    "avg_oracle_future_demand_coverage",
    "avg_historical_future_pearson",
    "avg_historical_future_topk_jaccard",
    "avg_demand_drift_ratio",
]


def read_one(path):
    rows = list(csv.DictReader(path.open(newline="")))
    if len(rows) != 1:
        raise ValueError(f"{path} has {len(rows)} rows, expected 1")
    return rows[0]


def read_many(path):
    return list(csv.DictReader(path.open(newline="")))


def parse_name(path):
    match = NAME_RE.match(path.stem)
    if not match:
        return None
    parsed = match.groupdict()
    parsed["seed"] = int(parsed["seed"])
    parsed["load"] = float(parsed["load"].replace("p", "."))
    return parsed


def to_float(value):
    if value is None or value == "":
        return float("nan")
    return float(value)


def to_int(value):
    if value is None or value == "":
        return 0
    return int(float(value))


def average(values):
    finite = [value for value in values if not math.isnan(value)]
    return sum(finite) / len(finite) if finite else float("nan")


def percentile_nearest_rank(values, percentile):
    if not values:
        return float("nan")
    ordered = sorted(values)
    rank = max(1, math.ceil(percentile * len(ordered)))
    return ordered[rank - 1]


def fmt(value):
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, float):
        if math.isnan(value):
            return ""
        return f"{value:.12g}"
    return str(value)


def aggregate_scheduling(path, parsed):
    if not path.exists():
        return None, []
    rows = []
    for row in read_many(path):
        rows.append(
            {
                "scenario": parsed["scenario"],
                "scheme": parsed["scheme"],
                "seed": parsed["seed"],
                "offered_load_factor": parsed["load"],
                "cycle": to_int(row.get("cycle")),
                "round_start_s": to_float(row.get("round_start_s")),
                "round_end_s": to_float(row.get("round_end_s")),
                "observed_matrix_bytes": to_int(row.get("observed_matrix_bytes")),
                "future_demand_bytes": to_int(row.get("future_demand_bytes")),
                "selected_edge_count": to_int(row.get("selected_edge_count")),
                "active_edge_count": to_int(row.get("active_edge_count")),
                "selected_hit_flows": to_int(row.get("selected_hit_flows")),
                "selected_hit_bytes": to_int(row.get("selected_hit_bytes")),
                "selected_but_unused_lightpaths": to_int(row.get("selected_but_unused_lightpaths")),
                "selected_edge_jaccard": to_float(row.get("selected_edge_jaccard")),
                "selected_oracle_jaccard": to_float(row.get("selected_oracle_jaccard")),
                "volume_oracle_jaccard": to_float(row.get("volume_oracle_jaccard")),
                "tl_oracle_jaccard": to_float(row.get("tl_oracle_jaccard")),
                "volume_future_demand_coverage": to_float(row.get("volume_future_demand_coverage")),
                "tl_future_demand_coverage": to_float(row.get("tl_future_demand_coverage")),
                "oracle_future_demand_coverage": to_float(row.get("oracle_future_demand_coverage")),
                "historical_future_pearson": to_float(row.get("historical_future_pearson")),
                "historical_future_topk_jaccard": to_float(row.get("historical_future_topk_jaccard")),
                "demand_drift_ratio": to_float(row.get("demand_drift_ratio")),
                "volume_oracle_possible_bytes_missed": to_int(row.get("volume_oracle_possible_bytes_missed")),
                "tl_oracle_possible_bytes_missed": to_int(row.get("tl_oracle_possible_bytes_missed")),
                "volume_selected_edges": row.get("volume_selected_edges", ""),
                "tl_ocs_selected_edges": row.get("tl_ocs_selected_edges", ""),
                "oracle_selected_edges": row.get("oracle_selected_edges", ""),
                "raw_a_top_edges": row.get("raw_a_top_edges", ""),
                "tl_g_top_edges": row.get("tl_g_top_edges", ""),
                "future_demand_top_edges": row.get("future_demand_top_edges", ""),
            }
        )
    if not rows:
        return None, []
    summary = {
        "avg_volume_tl_jaccard": average([row["selected_edge_jaccard"] for row in rows]),
        "avg_volume_oracle_jaccard": average([row["volume_oracle_jaccard"] for row in rows]),
        "avg_tl_oracle_jaccard": average([row["tl_oracle_jaccard"] for row in rows]),
        "avg_selected_oracle_jaccard": average([row["selected_oracle_jaccard"] for row in rows]),
        "avg_volume_future_demand_coverage": average([row["volume_future_demand_coverage"] for row in rows]),
        "avg_tl_future_demand_coverage": average([row["tl_future_demand_coverage"] for row in rows]),
        "avg_oracle_future_demand_coverage": average([row["oracle_future_demand_coverage"] for row in rows]),
        "avg_historical_future_pearson": average([row["historical_future_pearson"] for row in rows]),
        "avg_historical_future_topk_jaccard": average([row["historical_future_topk_jaccard"] for row in rows]),
        "avg_demand_drift_ratio": average([row["demand_drift_ratio"] for row in rows]),
        "selected_but_unused_lightpaths": sum(row["selected_but_unused_lightpaths"] for row in rows),
        "volume_oracle_possible_bytes_missed": sum(row["volume_oracle_possible_bytes_missed"] for row in rows),
        "tl_oracle_possible_bytes_missed": sum(row["tl_oracle_possible_bytes_missed"] for row in rows),
    }
    return summary, rows


def aggregate_run(summary_path):
    parsed = parse_name(summary_path)
    if parsed is None:
        return None, []
    flow_path = summary_path.with_name(f"{summary_path.stem}-flows.csv")
    if not flow_path.exists():
        raise FileNotFoundError(flow_path)
    summary = read_one(summary_path)
    flows = read_many(flow_path)
    stop_time = to_float(summary["stop_time_s"])
    total_sent = sum(to_int(row["size_bytes"]) for row in flows)
    total_received = sum(to_int(row["received_bytes"]) for row in flows)
    completed_fcts = [
        to_float(row["fct_s"])
        for row in flows
        if row.get("completed") == "true" and row.get("fct_s")
    ]
    completed = len(completed_fcts)
    ocs_path_count = sum(1 for row in flows if row.get("path_type") == "ocs")
    eps_path_count = sum(1 for row in flows if row.get("path_type") == "eps")
    ocs_bytes = sum(to_int(row["size_bytes"]) for row in flows if row.get("path_type") == "ocs")
    sched_summary, sched_rows = aggregate_scheduling(
        summary_path.with_name(f"{summary_path.stem}-scheduling.csv"),
        parsed,
    )
    if sched_summary is None:
        sched_summary = {key: float("nan") for key in SCHED_SUMMARY_DEFAULTS}
        sched_summary.update(
            {
                "selected_but_unused_lightpaths": 0,
                "volume_oracle_possible_bytes_missed": 0,
                "tl_oracle_possible_bytes_missed": 0,
            }
        )
    row = {
        "scenario": parsed["scenario"],
        "scheme": parsed["scheme"],
        "seed": parsed["seed"],
        "offered_load_factor": parsed["load"],
        "stop_time_s": stop_time,
        "flow_count": len(flows),
        "completed_flow_count": completed,
        "incomplete_flow_count": len(flows) - completed,
        "completion_ratio": completed / len(flows) if flows else 0.0,
        "total_sent_bytes": total_sent,
        "total_received_bytes": total_received,
        "actual_offered_bps": total_sent * 8.0 / stop_time if stop_time > 0 else float("nan"),
        "actual_received_bps": total_received * 8.0 / stop_time if stop_time > 0 else float("nan"),
        "avg_received_throughput_bps": total_received * 8.0 / stop_time if stop_time > 0 else float("nan"),
        "avg_fct_s": average(completed_fcts),
        "p95_fct_s": percentile_nearest_rank(completed_fcts, 0.95),
        "eps_path_count": eps_path_count,
        "ocs_path_count": ocs_path_count,
        "ocs_assigned_flows": ocs_path_count,
        "ocs_assigned_bytes": ocs_bytes,
        "ocs_byte_hit_rate": ocs_bytes / total_sent if total_sent else 0.0,
        "eps_avg_link_utilization": to_float(summary.get("eps_avg_link_utilization")),
        "eps_max_link_utilization": to_float(summary.get("eps_max_link_utilization")),
    }
    row.update(sched_summary)
    return row, sched_rows


def flow_signature(summary_path):
    rows = read_many(summary_path.with_name(f"{summary_path.stem}-flows.csv"))
    rows.sort(key=lambda row: to_int(row.get("flow_id")))
    return [
        (
            row.get("flow_id"),
            row.get("source_tor"),
            row.get("destination_tor"),
            row.get("size_bytes"),
            row.get("start_time_s"),
        )
        for row in rows
    ]


def write_csv(path, fields, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow({field: fmt(row.get(field, "")) for field in fields})


def load_audit_summary():
    if not AUDIT_IN.exists():
        return {}
    groups = defaultdict(list)
    with AUDIT_IN.open(newline="") as stream:
        for row in csv.DictReader(stream):
            key = (row["scenario"], int(row["seed"]), float(row["load"]))
            groups[key].append(row)
    summaries = {}
    for key, rows in groups.items():
        summaries[key] = {
            "avg_corr_target_matrix_d_future": average(
                [to_float(row.get("corr_target_matrix_d_future")) for row in rows]
            ),
            "avg_target_d_future_matrix_error": average(
                [to_float(row.get("normalized_matrix_error_target_d_future")) for row in rows]
            ),
            "replay_audit_gate_pass": all(row.get("gate_pass") == "true" for row in rows),
        }
    return summaries


def build_closeness(summary_rows):
    by_key = defaultdict(dict)
    for row in summary_rows:
        key = (row["scenario"], row["seed"], row["offered_load_factor"])
        by_key[key][row["scheme"]] = row
    rows = []
    for (scenario, seed, load), schemes in sorted(by_key.items()):
        oracle = schemes.get("ocs-oracle")
        if oracle is None:
            continue
        for scheme in ["eps-ecmp", "ocs-volume", "tl-ocs"]:
            row = schemes.get(scheme)
            if row is None:
                continue
            coverage = row.get("avg_volume_future_demand_coverage")
            missed = row.get("volume_oracle_possible_bytes_missed")
            if scheme == "tl-ocs":
                coverage = row.get("avg_tl_future_demand_coverage")
                missed = row.get("tl_oracle_possible_bytes_missed")
            rows.append(
                {
                    "scenario": scenario,
                    "seed": seed,
                    "offered_load_factor": load,
                    "scheme": scheme,
                    "avg_fct_s": row["avg_fct_s"],
                    "oracle_avg_fct_s": oracle["avg_fct_s"],
                    "avg_fct_ratio_vs_oracle": row["avg_fct_s"] / oracle["avg_fct_s"]
                    if oracle["avg_fct_s"] and not math.isnan(oracle["avg_fct_s"])
                    else float("nan"),
                    "p95_fct_s": row["p95_fct_s"],
                    "oracle_p95_fct_s": oracle["p95_fct_s"],
                    "p95_fct_ratio_vs_oracle": row["p95_fct_s"] / oracle["p95_fct_s"]
                    if oracle["p95_fct_s"] and not math.isnan(oracle["p95_fct_s"])
                    else float("nan"),
                    "completion_ratio": row["completion_ratio"],
                    "oracle_completion_ratio": oracle["completion_ratio"],
                    "completion_ratio_gap_vs_oracle": oracle["completion_ratio"] - row["completion_ratio"],
                    "avg_received_throughput_bps": row["avg_received_throughput_bps"],
                    "oracle_avg_received_throughput_bps": oracle["avg_received_throughput_bps"],
                    "throughput_ratio_vs_oracle": row["avg_received_throughput_bps"]
                    / oracle["avg_received_throughput_bps"]
                    if oracle["avg_received_throughput_bps"]
                    and not math.isnan(oracle["avg_received_throughput_bps"])
                    else float("nan"),
                    "avg_selected_oracle_jaccard": row.get("avg_selected_oracle_jaccard", float("nan")),
                    "avg_future_demand_coverage": coverage,
                    "oracle_future_demand_coverage": row.get("avg_oracle_future_demand_coverage"),
                    "oracle_possible_bytes_missed": missed,
                }
            )
    return rows


def quality_checks(summary_rows, audit_summary):
    errors = []
    warnings = []
    by_group = defaultdict(list)
    for row in summary_rows:
        by_group[(row["scenario"], row["scheme"], row["seed"])].append(row)
        if row["scheme"] == "eps-ecmp" and row["ocs_path_count"] != 0:
            errors.append(f"eps-ecmp has OCS paths in {row['scenario']} load {row['offered_load_factor']}")
        if row["scheme"] != "eps-ecmp" and math.isnan(row.get("avg_volume_tl_jaccard", float("nan"))):
            errors.append(f"missing scheduling diagnostics for {row['scenario']} {row['scheme']} load {row['offered_load_factor']}")
    for key, rows in by_group.items():
        ordered = sorted(rows, key=lambda row: row["offered_load_factor"])
        bytes_by_load = [row["total_sent_bytes"] for row in ordered]
        if any(right <= left for left, right in zip(bytes_by_load, bytes_by_load[1:])):
            errors.append(f"non-increasing total_sent_bytes for {key}: {bytes_by_load}")

    for key, audit in audit_summary.items():
        if not audit["replay_audit_gate_pass"]:
            warnings.append(f"replay audit gate failed for {key}")

    summaries = {path.stem: path for path in RAW_DIR.glob("phase15h-r4-*.csv") if not path.stem.endswith("-flows") and not path.stem.endswith("-scheduling")}
    by_flow_key = defaultdict(dict)
    for stem, path in summaries.items():
        parsed = parse_name(path)
        if parsed is None:
            continue
        key = (parsed["scenario"], parsed["seed"], parsed["load"])
        by_flow_key[key][parsed["scheme"]] = path
    for key, paths in by_flow_key.items():
        reference = None
        for scheme in ["eps-ecmp", "ocs-volume", "tl-ocs", "ocs-oracle"]:
            path = paths.get(scheme)
            if path is None:
                errors.append(f"missing {scheme} run for {key}")
                continue
            signature = flow_signature(path)
            if reference is None:
                reference = signature
            elif signature != reference:
                errors.append(f"flow sequence mismatch for {key} scheme {scheme}")
    return errors, warnings


def main():
    audit_summary = load_audit_summary()
    summary_rows = []
    scheduling_rows = []
    for path in sorted(RAW_DIR.glob("phase15h-r4-*.csv")):
        if path.stem.endswith("-flows") or path.stem.endswith("-scheduling"):
            continue
        parsed = parse_name(path)
        if parsed is None:
            continue
        row, sched = aggregate_run(path)
        audit = audit_summary.get((row["scenario"], row["seed"], row["offered_load_factor"]), {})
        row.update(
            {
                "avg_corr_target_matrix_d_future": audit.get(
                    "avg_corr_target_matrix_d_future", float("nan")
                ),
                "avg_target_d_future_matrix_error": audit.get(
                    "avg_target_d_future_matrix_error", float("nan")
                ),
                "replay_audit_gate_pass": audit.get("replay_audit_gate_pass", False),
            }
        )
        summary_rows.append(row)
        scheduling_rows.extend(sched)

    write_csv(SUMMARY_OUT, SUMMARY_FIELDS, summary_rows)
    write_csv(SCHED_OUT, SCHED_FIELDS, scheduling_rows)
    closeness_rows = build_closeness(summary_rows)
    write_csv(CLOSENESS_OUT, CLOSENESS_FIELDS, closeness_rows)

    errors, warnings = quality_checks(summary_rows, audit_summary)
    quality = [
        {
            "status": "failed" if errors else "passed",
            "observed_summary_count": len(summary_rows),
            "observed_scheduling_row_count": len(scheduling_rows),
            "oracle_closeness_row_count": len(closeness_rows),
            "error_count": len(errors),
            "warning_count": len(warnings),
            "errors": " | ".join(errors),
            "warnings": " | ".join(warnings),
        }
    ]
    write_csv(
        QUALITY_OUT,
        [
            "status",
            "observed_summary_count",
            "observed_scheduling_row_count",
            "oracle_closeness_row_count",
            "error_count",
            "warning_count",
            "errors",
            "warnings",
        ],
        quality,
    )
    print(f"wrote {SUMMARY_OUT}")
    print(f"wrote {QUALITY_OUT}")
    print(f"wrote {SCHED_OUT}")
    print(f"wrote {CLOSENESS_OUT}")
    print(f"quality status: {quality[0]['status']}")


if __name__ == "__main__":
    main()
