#!/usr/bin/env python3
import csv
import math
import re
from collections import defaultdict
from pathlib import Path

RAW_DIR = Path("results/raw")
PROCESSED_DIR = Path("results/processed")
SUMMARY_OUT = PROCESSED_DIR / "phase15h-r1-datapath-summary.csv"
QUALITY_OUT = PROCESSED_DIR / "phase15h-r1-datapath-quality-report.csv"
MAX_GENERATED_FLOWS = 100000

NAME_RE = re.compile(
    r"^phase15h-r1-(?P<scenario>single-pair-heavy|near-neighbor-heavy)-"
    r"(?P<scheme>force-eps|force-ocs|eps-ecmp|ocs-volume)-"
    r"seed(?P<seed>\d+)-load(?P<load>[0-9]+p[0-9]+)$"
)

SUMMARY_FIELDS = [
    "scenario",
    "scheme",
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
    "actual_offered_bytes",
    "actual_offered_bps",
    "avg_received_throughput_bps",
    "avg_fct_s",
    "p95_fct_s",
    "eps_path_count",
    "ocs_path_count",
    "ocs_assigned_flows",
    "ocs_assigned_bytes",
    "summary_status",
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
    parsed["seed"] = int(parsed["seed"])
    parsed["load"] = float(parsed["load"].replace("p", "."))
    return parsed


def to_float(value):
    if value is None or value == "":
        return float("nan")
    return float(value)


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


def aggregate_run(summary_path):
    parsed = parse_name(summary_path)
    if parsed is None:
        return None
    flow_path = summary_path.with_name(f"{summary_path.stem}-flows.csv")
    if not flow_path.exists():
        raise FileNotFoundError(flow_path)

    summary = read_one(summary_path)
    flows = read_many(flow_path)
    stop_time_s = to_float(summary["stop_time_s"])
    total_sent = sum(int(row["size_bytes"]) for row in flows)
    total_received = sum(int(row["received_bytes"]) for row in flows)
    completed_fcts = [
        to_float(row["fct_s"])
        for row in flows
        if row["completed"] == "true" and row.get("fct_s", "") != ""
    ]
    completed = len(completed_fcts)
    ocs_path_count = sum(1 for row in flows if row["path_type"] == "ocs")
    eps_path_count = sum(1 for row in flows if row["path_type"] == "eps")
    ocs_assigned_bytes = sum(
        int(row["size_bytes"]) for row in flows if row["path_type"] == "ocs"
    )

    return {
        "scenario": parsed["scenario"],
        "scheme": parsed["scheme"],
        "diagnostic_only": parsed["scheme"].startswith("force-"),
        "seed": parsed["seed"],
        "offered_load_factor": parsed["load"],
        "stop_time_s": stop_time_s,
        "flow_count": len(flows),
        "completed_flow_count": completed,
        "incomplete_flow_count": len(flows) - completed,
        "completion_ratio": completed / len(flows) if flows else float("nan"),
        "total_sent_bytes": total_sent,
        "total_received_bytes": total_received,
        "actual_offered_bytes": total_sent,
        "actual_offered_bps": total_sent * 8.0 / stop_time_s if stop_time_s > 0 else float("nan"),
        "avg_received_throughput_bps": total_received * 8.0 / stop_time_s
        if stop_time_s > 0
        else float("nan"),
        "avg_fct_s": sum(completed_fcts) / completed if completed else float("nan"),
        "p95_fct_s": percentile_nearest_rank(completed_fcts, 0.95),
        "eps_path_count": eps_path_count,
        "ocs_path_count": ocs_path_count,
        "ocs_assigned_flows": int(summary["ocs_assigned_flows"] or 0),
        "ocs_assigned_bytes": ocs_assigned_bytes,
        "summary_status": summary["status"],
        "_flow_sequence": [
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
        ],
    }


def main():
    PROCESSED_DIR.mkdir(parents=True, exist_ok=True)
    rows = []
    errors = []
    warnings = []

    for summary_path in sorted(RAW_DIR.glob("phase15h-r1-*.csv")):
        if summary_path.name.endswith("-flows.csv") or summary_path.name.endswith("-scheduling.csv"):
            continue
        try:
            row = aggregate_run(summary_path)
            if row is not None:
                rows.append(row)
        except Exception as exc:
            errors.append(f"{summary_path}: {exc}")

    rows.sort(key=lambda row: (row["scenario"], row["scheme"], row["seed"], row["offered_load_factor"]))

    for row in rows:
        if row["flow_count"] >= MAX_GENERATED_FLOWS:
            warnings.append(
                f"{row['scenario']} {row['scheme']} load {row['offered_load_factor']}: "
                "maxGeneratedFlows safety cap reached"
            )
        if row["scheme"] in ("force-eps", "eps-ecmp") and row["ocs_path_count"] != 0:
            errors.append(f"{row['scenario']} {row['scheme']}: unexpected OCS paths")
        if row["scheme"] == "force-ocs" and row["ocs_path_count"] == 0:
            errors.append(f"{row['scenario']} force-ocs: no OCS paths assigned")
        if row["scheme"] == "force-ocs" and row["eps_path_count"] != 0:
            warnings.append(
                f"{row['scenario']} force-ocs load {row['offered_load_factor']}: "
                f"{row['eps_path_count']} EPS fallback flows"
            )

    grouped = defaultdict(list)
    for row in rows:
        grouped[(row["scenario"], row["scheme"], row["seed"])].append(row)
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

    quality = {
        "status": "failed" if errors else "passed",
        "observed_summary_count": len(rows),
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
    print(f"wrote {QUALITY_OUT}")
    if errors:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
