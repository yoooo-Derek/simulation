#!/usr/bin/env python3
"""Aggregate Phase 15H-R5 app-stop/load-normalization validation outputs."""

import csv
import math
import re
from collections import defaultdict
from pathlib import Path

RAW_DIR = Path("results/raw")
PROCESSED_DIR = Path("results/processed")
SUMMARY_OUT = PROCESSED_DIR / "phase15h-r5-mechanism-summary.csv"
DRAIN_OUT = PROCESSED_DIR / "phase15h-r5-drain-sensitivity.csv"
LOAD_OUT = PROCESSED_DIR / "phase15h-r5-normalized-load-summary.csv"
CLOSENESS_OUT = PROCESSED_DIR / "phase15h-r5-oracle-closeness.csv"
QUALITY_OUT = PROCESSED_DIR / "phase15h-r5-quality-report.csv"

NAME_RE = re.compile(
    r"^phase15h-r5-(?P<scenario>.+)-"
    r"(?P<scheme>eps-ecmp|ocs-volume|tl-ocs|ocs-oracle)-"
    r"seed(?P<seed>\d+)-load(?P<load>[0-9]+p[0-9]+)-(?P<drain>D[0-9]+)$"
)

SUMMARY_FIELDS = [
    "scenario",
    "scheme",
    "seed",
    "offered_load_factor",
    "drain_label",
    "traffic_stop_time_s",
    "sim_stop_time_s",
    "drain_time_s",
    "measurement_start_time_s",
    "measurement_end_time_s",
    "measurement_duration_s",
    "flow_count",
    "completed_flow_count",
    "incomplete_flow_count",
    "completion_ratio",
    "total_sent_bytes",
    "total_received_bytes",
    "offered_bytes_measurement",
    "cross_tor_offered_bytes_measurement",
    "actual_offered_bps",
    "actual_cross_tor_offered_bps",
    "actual_received_bps",
    "avg_received_throughput_bps",
    "normalized_access_load",
    "normalized_eps_load",
    "max_tor_offered_bps",
    "max_tor_offered_load_eps",
    "max_tor_offered_load_hybrid",
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
    "selected_but_unused_lightpaths",
    "volume_oracle_possible_bytes_missed",
    "tl_oracle_possible_bytes_missed",
]

SCHED_FIELDS = [
    "scenario",
    "scheme",
    "seed",
    "offered_load_factor",
    "drain_label",
    "cycle",
    "round_start_s",
    "round_end_s",
    "observed_matrix_bytes",
    "future_demand_bytes",
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
    "selected_but_unused_lightpaths",
    "volume_oracle_possible_bytes_missed",
    "tl_oracle_possible_bytes_missed",
]

DRAIN_FIELDS = [
    "scenario",
    "scheme",
    "seed",
    "offered_load_factor",
    "normalized_eps_load",
    "drain_label",
    "drain_time_s",
    "completion_ratio",
    "incomplete_flow_count",
    "avg_fct_s",
    "p95_fct_s",
    "avg_received_throughput_bps",
]

LOAD_FIELDS = [
    "scenario",
    "seed",
    "offered_load_factor",
    "drain_label",
    "traffic_stop_time_s",
    "measurement_duration_s",
    "offered_bytes_measurement",
    "cross_tor_offered_bytes_measurement",
    "actual_offered_bps",
    "actual_cross_tor_offered_bps",
    "normalized_access_load",
    "normalized_eps_load",
    "max_tor_offered_load_eps",
    "max_tor_offered_load_hybrid",
]

CLOSENESS_FIELDS = [
    "scenario",
    "seed",
    "offered_load_factor",
    "drain_label",
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
        return {}, []
    rows = []
    for row in read_many(path):
        rows.append(
            {
                "scenario": parsed["scenario"],
                "scheme": parsed["scheme"],
                "seed": parsed["seed"],
                "offered_load_factor": parsed["load"],
                "drain_label": parsed["drain"],
                "cycle": to_int(row.get("cycle")),
                "round_start_s": to_float(row.get("round_start_s")),
                "round_end_s": to_float(row.get("round_end_s")),
                "observed_matrix_bytes": to_int(row.get("observed_matrix_bytes")),
                "future_demand_bytes": to_int(row.get("future_demand_bytes")),
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
                "selected_but_unused_lightpaths": to_int(row.get("selected_but_unused_lightpaths")),
                "volume_oracle_possible_bytes_missed": to_int(row.get("volume_oracle_possible_bytes_missed")),
                "tl_oracle_possible_bytes_missed": to_int(row.get("tl_oracle_possible_bytes_missed")),
            }
        )
    if not rows:
        return {}, []
    summary = {
        "avg_volume_tl_jaccard": average([row["selected_edge_jaccard"] for row in rows]),
        "avg_volume_oracle_jaccard": average([row["volume_oracle_jaccard"] for row in rows]),
        "avg_tl_oracle_jaccard": average([row["tl_oracle_jaccard"] for row in rows]),
        "avg_selected_oracle_jaccard": average([row["selected_oracle_jaccard"] for row in rows]),
        "avg_volume_future_demand_coverage": average([row["volume_future_demand_coverage"] for row in rows]),
        "avg_tl_future_demand_coverage": average([row["tl_future_demand_coverage"] for row in rows]),
        "avg_oracle_future_demand_coverage": average([row["oracle_future_demand_coverage"] for row in rows]),
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
    completed_fcts = [
        to_float(row["fct_s"])
        for row in flows
        if row.get("completed") == "true" and row.get("fct_s")
    ]
    total_sent = sum(to_int(row["size_bytes"]) for row in flows)
    total_received = sum(to_int(row["received_bytes"]) for row in flows)
    completed = len(completed_fcts)
    ocs_path_count = sum(1 for row in flows if row.get("path_type") == "ocs")
    eps_path_count = sum(1 for row in flows if row.get("path_type") == "eps")
    ocs_bytes = sum(to_int(row["size_bytes"]) for row in flows if row.get("path_type") == "ocs")
    sched_summary, sched_rows = aggregate_scheduling(
        summary_path.with_name(f"{summary_path.stem}-scheduling.csv"),
        parsed,
    )

    row = {
        "scenario": parsed["scenario"],
        "scheme": parsed["scheme"],
        "seed": parsed["seed"],
        "offered_load_factor": parsed["load"],
        "drain_label": parsed["drain"],
        "traffic_stop_time_s": to_float(summary.get("traffic_stop_time_s")),
        "sim_stop_time_s": to_float(summary.get("sim_stop_time_s", summary.get("stop_time_s"))),
        "drain_time_s": to_float(summary.get("drain_time_s")),
        "measurement_start_time_s": to_float(summary.get("measurement_start_time_s")),
        "measurement_end_time_s": to_float(summary.get("measurement_end_time_s")),
        "measurement_duration_s": to_float(summary.get("measurement_duration_s")),
        "flow_count": len(flows),
        "completed_flow_count": completed,
        "incomplete_flow_count": len(flows) - completed,
        "completion_ratio": completed / len(flows) if flows else 0.0,
        "total_sent_bytes": total_sent,
        "total_received_bytes": total_received,
        "offered_bytes_measurement": to_int(summary.get("offered_bytes_measurement")),
        "cross_tor_offered_bytes_measurement": to_int(summary.get("cross_tor_offered_bytes_measurement")),
        "actual_offered_bps": to_float(summary.get("actual_offered_bps")),
        "actual_cross_tor_offered_bps": to_float(summary.get("actual_cross_tor_offered_bps")),
        "actual_received_bps": to_float(summary.get("actual_received_bps")),
        "avg_received_throughput_bps": to_float(summary.get("avg_received_throughput_bps")),
        "normalized_access_load": to_float(summary.get("normalized_access_load")),
        "normalized_eps_load": to_float(summary.get("normalized_eps_load")),
        "max_tor_offered_bps": to_float(summary.get("max_tor_offered_bps")),
        "max_tor_offered_load_eps": to_float(summary.get("max_tor_offered_load_eps")),
        "max_tor_offered_load_hybrid": to_float(summary.get("max_tor_offered_load_hybrid")),
        "avg_fct_s": average(completed_fcts),
        "p95_fct_s": percentile_nearest_rank(completed_fcts, 0.95),
        "eps_path_count": eps_path_count,
        "ocs_path_count": ocs_path_count,
        "ocs_assigned_flows": ocs_path_count,
        "ocs_assigned_bytes": ocs_bytes,
        "ocs_byte_hit_rate": ocs_bytes / total_sent if total_sent else 0.0,
        "eps_avg_link_utilization": to_float(summary.get("eps_avg_link_utilization")),
        "eps_max_link_utilization": to_float(summary.get("eps_max_link_utilization")),
        "avg_volume_tl_jaccard": float("nan"),
        "avg_volume_oracle_jaccard": float("nan"),
        "avg_tl_oracle_jaccard": float("nan"),
        "avg_selected_oracle_jaccard": float("nan"),
        "avg_volume_future_demand_coverage": float("nan"),
        "avg_tl_future_demand_coverage": float("nan"),
        "avg_oracle_future_demand_coverage": float("nan"),
        "selected_but_unused_lightpaths": 0,
        "volume_oracle_possible_bytes_missed": 0,
        "tl_oracle_possible_bytes_missed": 0,
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
        writer = csv.DictWriter(stream, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        for row in rows:
            writer.writerow({field: fmt(row.get(field, "")) for field in fields})


def build_closeness(summary_rows):
    by_key = defaultdict(dict)
    for row in summary_rows:
        key = (row["scenario"], row["seed"], row["offered_load_factor"], row["drain_label"])
        by_key[key][row["scheme"]] = row
    rows = []
    for (scenario, seed, load, drain), schemes in sorted(by_key.items()):
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
                    "drain_label": drain,
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


def quality_checks(summary_rows):
    errors = []
    warnings = []
    expected = {
        "eps-ecmp",
        "ocs-volume",
        "tl-ocs",
        "ocs-oracle",
    }
    by_key = defaultdict(dict)
    for row in summary_rows:
        key = (row["scenario"], row["seed"], row["offered_load_factor"], row["drain_label"])
        by_key[key][row["scheme"]] = row
        if math.isnan(row["normalized_eps_load"]) or math.isnan(row["normalized_access_load"]):
            errors.append(f"missing normalized load for {key} {row['scheme']}")
        if row["scheme"] == "eps-ecmp" and row["ocs_path_count"] != 0:
            errors.append(f"eps-ecmp has OCS paths in {key}")
        if row["scheme"] != "eps-ecmp" and math.isnan(row.get("avg_selected_oracle_jaccard", float("nan"))):
            errors.append(f"missing scheduling diagnostics for {key} {row['scheme']}")
    for key, schemes in by_key.items():
        missing = expected.difference(schemes)
        if missing:
            errors.append(f"missing schemes for {key}: {sorted(missing)}")
    for key, schemes in by_key.items():
        paths = {}
        scenario, seed, load, drain = key
        load_token = str(load).replace(".", "p")
        for scheme in expected:
            stem = f"phase15h-r5-{scenario}-{scheme}-seed{seed}-load{load_token}-{drain}"
            path = RAW_DIR / f"{stem}.csv"
            if path.exists():
                paths[scheme] = path
        reference = None
        for scheme in ["eps-ecmp", "ocs-volume", "tl-ocs", "ocs-oracle"]:
            path = paths.get(scheme)
            if path is None:
                continue
            signature = flow_signature(path)
            if reference is None:
                reference = signature
            elif signature != reference:
                errors.append(f"flow sequence mismatch for {key} scheme {scheme}")
    by_drain = defaultdict(dict)
    for row in summary_rows:
        by_drain[(row["scenario"], row["scheme"], row["seed"], row["offered_load_factor"])][
            row["drain_label"]
        ] = row
    for key, drains in by_drain.items():
        d0 = drains.get("D0")
        if d0 is None:
            continue
        for label in ["D1", "D2", "D3"]:
            row = drains.get(label)
            if row and row["completion_ratio"] + 1e-12 < d0["completion_ratio"]:
                errors.append(f"{key} {label} completion ratio lower than D0")
        if drains.get("D3") and drains["D3"]["completion_ratio"] < 0.95:
            warnings.append(f"{key} D3 completion ratio below 0.95")
    return errors, warnings


def main():
    summary_rows = []
    scheduling_rows = []
    for path in sorted(RAW_DIR.glob("phase15h-r5-*.csv")):
        if path.stem.endswith("-flows") or path.stem.endswith("-scheduling"):
            continue
        parsed = parse_name(path)
        if parsed is None:
            continue
        row, sched = aggregate_run(path)
        summary_rows.append(row)
        scheduling_rows.extend(sched)

    write_csv(SUMMARY_OUT, SUMMARY_FIELDS, summary_rows)
    write_csv(PROCESSED_DIR / "phase15h-r5-scheduling-diagnostics.csv", SCHED_FIELDS, scheduling_rows)
    write_csv(DRAIN_OUT, DRAIN_FIELDS, summary_rows)

    load_rows = []
    seen_load = set()
    for row in summary_rows:
        key = (row["scenario"], row["seed"], row["offered_load_factor"], row["drain_label"])
        if key in seen_load:
            continue
        seen_load.add(key)
        load_rows.append(row)
    write_csv(LOAD_OUT, LOAD_FIELDS, load_rows)

    closeness_rows = build_closeness(summary_rows)
    write_csv(CLOSENESS_OUT, CLOSENESS_FIELDS, closeness_rows)

    errors, warnings = quality_checks(summary_rows)
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
    for path in [SUMMARY_OUT, DRAIN_OUT, LOAD_OUT, CLOSENESS_OUT, QUALITY_OUT]:
        print(f"wrote {path}")
    print(f"quality status: {quality[0]['status']}")


if __name__ == "__main__":
    main()
