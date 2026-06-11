#!/usr/bin/env python3
import csv
import math
import re
from collections import defaultdict
from pathlib import Path

RAW_DIR = Path("results/raw")
PROCESSED_DIR = Path("results/processed")
SUMMARY_OUT = PROCESSED_DIR / "phase15h-r2-oracle-summary.csv"
QUALITY_OUT = PROCESSED_DIR / "phase15h-r2-oracle-quality-report.csv"
SCHEDULING_OUT = PROCESSED_DIR / "phase15h-r2-oracle-scheduling-diagnostics.csv"
COMPARISON_OUT = PROCESSED_DIR / "phase15h-r2-oracle-comparison.csv"
MAX_GENERATED_FLOWS = 100000

NAME_RE = re.compile(
    r"^phase15h-r2-(?P<scenario>.+)-"
    r"(?P<scheme>force-eps|force-ocs|eps-ecmp|ocs-volume|tl-ocs|ocs-oracle(?:-whole-run)?)-"
    r"seed(?P<seed>\d+)-load(?P<load>[0-9]+p[0-9]+)$"
)

SUMMARY_FIELDS = [
    "scenario",
    "scheme",
    "oracle_mode",
    "diagnostic_only",
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
    "avg_volume_oracle_jaccard",
    "avg_tl_oracle_jaccard",
    "avg_volume_tl_jaccard",
    "avg_selected_oracle_jaccard",
    "avg_selected_future_demand_coverage",
    "avg_volume_future_demand_coverage",
    "avg_tl_future_demand_coverage",
    "avg_oracle_future_demand_coverage",
    "selected_but_unused_lightpaths",
    "oracle_possible_ocs_bytes_missed",
    "volume_oracle_possible_bytes_missed",
    "tl_oracle_possible_bytes_missed",
    "summary_status",
]

SCHED_FIELDS = [
    "scenario",
    "scheme",
    "oracle_mode",
    "diagnostic_only",
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
    "oracle_possible_ocs_flows_missed",
    "oracle_possible_ocs_bytes_missed",
    "selected_oracle_jaccard",
    "volume_oracle_jaccard",
    "tl_oracle_jaccard",
    "selected_future_demand_coverage",
    "volume_future_demand_coverage",
    "tl_future_demand_coverage",
    "oracle_future_demand_coverage",
    "volume_oracle_possible_bytes_missed",
    "tl_oracle_possible_bytes_missed",
    "selected_edges",
    "volume_selected_edges",
    "tl_ocs_selected_edges",
    "oracle_selected_edges",
    "raw_a_top_edges",
    "tl_g_top_edges",
    "future_demand_top_edges",
]

COMPARISON_FIELDS = [
    "scenario",
    "seed",
    "offered_load_factor",
    "oracle_mode",
    "scheme",
    "oracle_scheme",
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
    "oracle_possible_ocs_bytes_missed",
]


def read_one(path):
    with path.open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    if len(rows) != 1:
        raise ValueError(f"{path} has {len(rows)} rows, expected 1")
    return rows[0]


def read_many(path):
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


def parse_name(path):
    match = NAME_RE.match(path.stem)
    if not match:
        return None
    parsed = match.groupdict()
    scheme = parsed["scheme"]
    oracle_mode = "period-future"
    if scheme == "ocs-oracle-whole-run":
        scheme = "ocs-oracle"
        oracle_mode = "whole-run"
    parsed["scheme"] = scheme
    parsed["oracle_mode"] = oracle_mode
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


def average(values):
    finite = [value for value in values if not math.isnan(value)]
    return sum(finite) / len(finite) if finite else float("nan")


def aggregate_scheduling(path, parsed):
    if not path.exists():
        return None, []
    rows = read_many(path)
    normalized = []
    for row in rows:
        normalized.append(
            {
                "scenario": parsed["scenario"],
                "scheme": parsed["scheme"],
                "oracle_mode": row.get("oracle_mode") or parsed["oracle_mode"],
                "diagnostic_only": row.get("diagnostic_only", "false") == "true",
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
                "oracle_possible_ocs_flows_missed": to_int(row.get("oracle_possible_ocs_flows_missed")),
                "oracle_possible_ocs_bytes_missed": to_int(row.get("oracle_possible_ocs_bytes_missed")),
                "selected_oracle_jaccard": to_float(row.get("selected_oracle_jaccard")),
                "volume_oracle_jaccard": to_float(row.get("volume_oracle_jaccard")),
                "tl_oracle_jaccard": to_float(row.get("tl_oracle_jaccard")),
                "selected_future_demand_coverage": to_float(row.get("selected_future_demand_coverage")),
                "volume_future_demand_coverage": to_float(row.get("volume_future_demand_coverage")),
                "tl_future_demand_coverage": to_float(row.get("tl_future_demand_coverage")),
                "oracle_future_demand_coverage": to_float(row.get("oracle_future_demand_coverage")),
                "volume_oracle_possible_bytes_missed": to_int(row.get("volume_oracle_possible_bytes_missed")),
                "tl_oracle_possible_bytes_missed": to_int(row.get("tl_oracle_possible_bytes_missed")),
                "selected_edges": row.get("selected_edges", ""),
                "volume_selected_edges": row.get("volume_selected_edges", ""),
                "tl_ocs_selected_edges": row.get("tl_ocs_selected_edges", ""),
                "oracle_selected_edges": row.get("oracle_selected_edges", ""),
                "raw_a_top_edges": row.get("raw_a_top_edges", ""),
                "tl_g_top_edges": row.get("tl_g_top_edges", ""),
                "future_demand_top_edges": row.get("future_demand_top_edges", ""),
            }
        )
    if not normalized:
        return None, []
    summary = {
        "avg_volume_oracle_jaccard": average([row["volume_oracle_jaccard"] for row in normalized]),
        "avg_tl_oracle_jaccard": average([row["tl_oracle_jaccard"] for row in normalized]),
        "avg_volume_tl_jaccard": average([to_float(row.get("selected_edge_jaccard")) for row in rows]),
        "avg_selected_oracle_jaccard": average([row["selected_oracle_jaccard"] for row in normalized]),
        "avg_selected_future_demand_coverage": average([row["selected_future_demand_coverage"] for row in normalized]),
        "avg_volume_future_demand_coverage": average([row["volume_future_demand_coverage"] for row in normalized]),
        "avg_tl_future_demand_coverage": average([row["tl_future_demand_coverage"] for row in normalized]),
        "avg_oracle_future_demand_coverage": average([row["oracle_future_demand_coverage"] for row in normalized]),
        "selected_but_unused_lightpaths": sum(row["selected_but_unused_lightpaths"] for row in normalized),
        "oracle_possible_ocs_bytes_missed": sum(row["oracle_possible_ocs_bytes_missed"] for row in normalized),
        "volume_oracle_possible_bytes_missed": sum(row["volume_oracle_possible_bytes_missed"] for row in normalized),
        "tl_oracle_possible_bytes_missed": sum(row["tl_oracle_possible_bytes_missed"] for row in normalized),
    }
    return summary, normalized


def aggregate_run(summary_path):
    parsed = parse_name(summary_path)
    if parsed is None:
        return None, []
    flow_path = summary_path.with_name(f"{summary_path.stem}-flows.csv")
    if not flow_path.exists():
        raise FileNotFoundError(flow_path)

    summary = read_one(summary_path)
    flows = read_many(flow_path)
    stop_time_s = to_float(summary["stop_time_s"])
    total_sent = sum(to_int(row["size_bytes"]) for row in flows)
    total_received = sum(to_int(row["received_bytes"]) for row in flows)
    completed_fcts = [
        to_float(row["fct_s"])
        for row in flows
        if row["completed"] == "true" and row.get("fct_s", "") != ""
    ]
    completed = len(completed_fcts)
    ocs_path_count = sum(1 for row in flows if row["path_type"] == "ocs")
    eps_path_count = sum(1 for row in flows if row["path_type"] == "eps")
    ocs_assigned_bytes = sum(
        to_int(row["size_bytes"]) for row in flows if row["path_type"] == "ocs"
    )
    scheduling_summary, scheduling_rows = aggregate_scheduling(
        summary_path.with_name(f"{summary_path.stem}-scheduling.csv"),
        parsed,
    )
    if scheduling_summary is None:
        scheduling_summary = {
            "avg_volume_oracle_jaccard": float("nan"),
            "avg_tl_oracle_jaccard": float("nan"),
            "avg_volume_tl_jaccard": float("nan"),
            "avg_selected_oracle_jaccard": float("nan"),
            "avg_selected_future_demand_coverage": float("nan"),
            "avg_volume_future_demand_coverage": float("nan"),
            "avg_tl_future_demand_coverage": float("nan"),
            "avg_oracle_future_demand_coverage": float("nan"),
            "selected_but_unused_lightpaths": 0,
            "oracle_possible_ocs_bytes_missed": 0,
            "volume_oracle_possible_bytes_missed": 0,
            "tl_oracle_possible_bytes_missed": 0,
        }

    row = {
        "scenario": parsed["scenario"],
        "scheme": parsed["scheme"],
        "oracle_mode": parsed["oracle_mode"],
        "diagnostic_only": parsed["scheme"] in {"force-eps", "force-ocs", "ocs-oracle"},
        "seed": parsed["seed"],
        "offered_load_factor": parsed["load"],
        "stop_time_s": stop_time_s,
        "flow_count": len(flows),
        "completed_flow_count": completed,
        "incomplete_flow_count": len(flows) - completed,
        "completion_ratio": completed / len(flows) if flows else float("nan"),
        "total_sent_bytes": total_sent,
        "total_received_bytes": total_received,
        "actual_offered_bps": total_sent * 8.0 / stop_time_s if stop_time_s > 0 else float("nan"),
        "actual_received_bps": total_received * 8.0 / stop_time_s if stop_time_s > 0 else float("nan"),
        "avg_received_throughput_bps": total_received * 8.0 / stop_time_s
        if stop_time_s > 0
        else float("nan"),
        "avg_fct_s": sum(completed_fcts) / completed if completed else float("nan"),
        "p95_fct_s": percentile_nearest_rank(completed_fcts, 0.95),
        "eps_path_count": eps_path_count,
        "ocs_path_count": ocs_path_count,
        "ocs_assigned_flows": to_int(summary.get("ocs_assigned_flows")),
        "ocs_assigned_bytes": ocs_assigned_bytes,
        "ocs_byte_hit_rate": ocs_assigned_bytes / total_sent if total_sent else float("nan"),
        "eps_avg_link_utilization": to_float(summary.get("eps_avg_link_utilization")),
        "eps_max_link_utilization": to_float(summary.get("eps_max_link_utilization")),
        "summary_status": summary["status"],
        "_flow_sequence": sorted([
            (
                row["flow_id"],
                row["source_tor"],
                row["source_server"],
                row["destination_tor"],
                row["destination_server"],
                row["size_bytes"],
                row["start_time_s"],
            )
            for row in flows
        ], key=lambda item: int(item[0])),
    }
    row.update(scheduling_summary)
    return row, scheduling_rows


def build_comparisons(rows):
    by_key = defaultdict(dict)
    for row in rows:
        by_key[(row["scenario"], row["seed"], row["offered_load_factor"])][
            (row["scheme"], row["oracle_mode"])
        ] = row
    comparisons = []
    for key, schemes in by_key.items():
        oracle = schemes.get(("ocs-oracle", "period-future"))
        if oracle is None:
            continue
        for (scheme, oracle_mode), row in schemes.items():
            if scheme == "ocs-oracle" and oracle_mode == "period-future":
                continue
            comparisons.append(
                {
                    "scenario": key[0],
                    "seed": key[1],
                    "offered_load_factor": key[2],
                    "oracle_mode": "period-future",
                    "scheme": scheme if oracle_mode == "period-future" else f"{scheme}-{oracle_mode}",
                    "oracle_scheme": "ocs-oracle",
                    "avg_fct_s": row["avg_fct_s"],
                    "oracle_avg_fct_s": oracle["avg_fct_s"],
                    "avg_fct_ratio_vs_oracle": row["avg_fct_s"] / oracle["avg_fct_s"]
                    if oracle["avg_fct_s"] and not math.isnan(row["avg_fct_s"]) and not math.isnan(oracle["avg_fct_s"])
                    else float("nan"),
                    "p95_fct_s": row["p95_fct_s"],
                    "oracle_p95_fct_s": oracle["p95_fct_s"],
                    "p95_fct_ratio_vs_oracle": row["p95_fct_s"] / oracle["p95_fct_s"]
                    if oracle["p95_fct_s"] and not math.isnan(row["p95_fct_s"]) and not math.isnan(oracle["p95_fct_s"])
                    else float("nan"),
                    "completion_ratio": row["completion_ratio"],
                    "oracle_completion_ratio": oracle["completion_ratio"],
                    "completion_ratio_gap_vs_oracle": row["completion_ratio"] - oracle["completion_ratio"],
                    "avg_received_throughput_bps": row["avg_received_throughput_bps"],
                    "oracle_avg_received_throughput_bps": oracle["avg_received_throughput_bps"],
                    "throughput_ratio_vs_oracle": row["avg_received_throughput_bps"]
                    / oracle["avg_received_throughput_bps"]
                    if oracle["avg_received_throughput_bps"]
                    and not math.isnan(row["avg_received_throughput_bps"])
                    and not math.isnan(oracle["avg_received_throughput_bps"])
                    else float("nan"),
                    "avg_selected_oracle_jaccard": row["avg_selected_oracle_jaccard"],
                    "oracle_possible_ocs_bytes_missed": row["oracle_possible_ocs_bytes_missed"],
                }
            )
    return comparisons


def main():
    PROCESSED_DIR.mkdir(parents=True, exist_ok=True)
    rows = []
    scheduling_rows = []
    errors = []
    warnings = []

    for summary_path in sorted(RAW_DIR.glob("phase15h-r2-*.csv")):
        if summary_path.name.endswith("-flows.csv") or summary_path.name.endswith("-scheduling.csv"):
            continue
        try:
            row, run_scheduling_rows = aggregate_run(summary_path)
            if row is not None:
                rows.append(row)
                scheduling_rows.extend(run_scheduling_rows)
        except Exception as exc:
            errors.append(f"{summary_path}: {exc}")

    rows.sort(key=lambda row: (row["scenario"], row["scheme"], row["oracle_mode"], row["seed"], row["offered_load_factor"]))
    scheduling_rows.sort(key=lambda row: (row["scenario"], row["scheme"], row["oracle_mode"], row["seed"], row["offered_load_factor"], row["cycle"]))

    for row in rows:
        if row["flow_count"] >= MAX_GENERATED_FLOWS:
            warnings.append(
                f"{row['scenario']} {row['scheme']} load {row['offered_load_factor']}: "
                "maxGeneratedFlows safety cap reached"
            )
        if row["scheme"] in ("force-eps", "eps-ecmp") and row["ocs_path_count"] != 0:
            errors.append(f"{row['scenario']} {row['scheme']}: unexpected OCS paths")
        if row["scheme"] == "ocs-oracle" and row["ocs_path_count"] == 0:
            warnings.append(
                f"{row['scenario']} ocs-oracle load {row['offered_load_factor']}: no OCS paths"
            )
        if row["scheme"] in ("ocs-volume", "tl-ocs", "ocs-oracle") and math.isnan(row["avg_selected_oracle_jaccard"]):
            errors.append(f"{row['scenario']} {row['scheme']} load {row['offered_load_factor']}: missing scheduling diagnostics")

    grouped = defaultdict(list)
    for row in rows:
        grouped[(row["scenario"], row["scheme"], row["oracle_mode"], row["seed"])].append(row)
    for key, group in grouped.items():
        ordered = sorted(group, key=lambda row: row["offered_load_factor"])
        for previous, current in zip(ordered, ordered[1:]):
            if current["total_sent_bytes"] < previous["total_sent_bytes"]:
                errors.append(f"{key}: total_sent_bytes decreased with load")
            elif current["total_sent_bytes"] == previous["total_sent_bytes"]:
                warnings.append(f"{key}: adjacent load has duplicate total_sent_bytes")
            if current["actual_offered_bps"] <= previous["actual_offered_bps"]:
                warnings.append(f"{key}: actual_offered_bps did not increase")

    sequence_groups = defaultdict(dict)
    for row in rows:
        if row["oracle_mode"] != "period-future":
            continue
        sequence_groups[(row["scenario"], row["seed"], row["offered_load_factor"])][
            row["scheme"]
        ] = row["_flow_sequence"]
    for key, schemes in sequence_groups.items():
        if len(schemes) < 2:
            continue
        reference_scheme = sorted(schemes)[0]
        reference = schemes[reference_scheme]
        for scheme, sequence in schemes.items():
            if sequence != reference:
                errors.append(f"{key}: flow sequence mismatch for {scheme} vs {reference_scheme}")

    with SUMMARY_OUT.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=SUMMARY_FIELDS)
        writer.writeheader()
        for row in rows:
            writer.writerow({field: fmt(row[field]) for field in SUMMARY_FIELDS})

    with SCHEDULING_OUT.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=SCHED_FIELDS)
        writer.writeheader()
        for row in scheduling_rows:
            writer.writerow({field: fmt(row[field]) for field in SCHED_FIELDS})

    comparisons = build_comparisons(rows)
    with COMPARISON_OUT.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=COMPARISON_FIELDS)
        writer.writeheader()
        for row in comparisons:
            writer.writerow({field: fmt(row[field]) for field in COMPARISON_FIELDS})

    quality = {
        "status": "failed" if errors else "passed",
        "observed_summary_count": len(rows),
        "observed_scheduling_row_count": len(scheduling_rows),
        "comparison_row_count": len(comparisons),
        "error_count": len(errors),
        "warning_count": len(warnings),
        "errors": " | ".join(errors),
        "warnings": " | ".join(warnings),
    }
    with QUALITY_OUT.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(quality))
        writer.writeheader()
        writer.writerow(quality)

    print(f"wrote {SUMMARY_OUT}")
    print(f"wrote {SCHEDULING_OUT}")
    print(f"wrote {COMPARISON_OUT}")
    print(f"wrote {QUALITY_OUT}")
    if errors:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
