#!/usr/bin/env python3
"""Prepare and aggregate Phase 15H-R6 FCT-oracle diagnostic runs."""

import argparse
import csv
import math
import re
from collections import defaultdict
from pathlib import Path

RAW_DIR = Path("results/raw")
PROCESSED_DIR = Path("results/processed")
PLAN_OUT = PROCESSED_DIR / "phase15h-r6-fct-oracle-candidate-plan.csv"
CANDIDATES_OUT = PROCESSED_DIR / "phase15h-r6-fct-oracle-candidates.csv"
ORACLE_OUT = PROCESSED_DIR / "phase15h-r6-fct-oracle-summary.csv"
BASELINE_OUT = PROCESSED_DIR / "phase15h-r6-baseline-summary.csv"
ATTR_FLOWS_OUT = PROCESSED_DIR / "phase15h-r6-flow-attribution-flows.csv"
ATTR_SUMMARY_OUT = PROCESSED_DIR / "phase15h-r6-flow-attribution-summary.csv"
ATTR_EXPLANATION_OUT = PROCESSED_DIR / "phase15h-r6-flow-attribution-explanation.csv"
QUALITY_OUT = PROCESSED_DIR / "phase15h-r6-quality-report.csv"

BASELINE_SCHEMES = ("eps-ecmp", "ocs-volume", "tl-ocs")
EDGE_RE = re.compile(r"(\d+)-(\d+)")


def load_token(load):
    return str(load).replace(".", "p")


def baseline_stem(scenario, scheme, seed, load, drain):
    return f"phase15h-r6-{scenario}-{scheme}-seed{seed}-load{load_token(load)}-{drain}"


def candidate_stem(scenario, seed, load, drain, candidate_id):
    return f"phase15h-r6-{scenario}-fixed-ocs-seed{seed}-load{load_token(load)}-{drain}-{candidate_id}"


def read_rows(path):
    if not path.exists():
        return []
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


def read_one(path):
    rows = read_rows(path)
    if len(rows) != 1:
        raise ValueError(f"{path} has {len(rows)} rows, expected 1")
    return rows[0]


def write_csv(path, fields, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        for row in rows:
            writer.writerow({field: fmt(row.get(field, "")) for field in fields})


def to_float(value):
    if value is None or value == "":
        return float("nan")
    return float(value)


def to_int(value):
    if value is None or value == "":
        return 0
    return int(float(value))


def fmt(value):
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, float):
        if math.isnan(value):
            return ""
        return f"{value:.12g}"
    return str(value)


def parse_edges(value):
    edges = set()
    if not value:
        return edges
    for left, right in EDGE_RE.findall(value):
        a = int(left)
        b = int(right)
        if a != b:
            edges.add((min(a, b), max(a, b)))
    return edges


def format_edges(edges):
    return ";".join(f"{a}-{b}" for a, b in sorted(edges))


def all_matchings(nodes):
    nodes = tuple(nodes)
    if not nodes:
        yield tuple()
        return
    first = nodes[0]
    rest = nodes[1:]
    for matching in all_matchings(rest):
        yield matching
    for index, second in enumerate(rest):
        remaining = rest[:index] + rest[index + 1 :]
        for matching in all_matchings(remaining):
            edge = (min(first, second), max(first, second))
            yield tuple(sorted((edge,) + matching))


def jaccard(left, right):
    left = set(left)
    right = set(right)
    if not left and not right:
        return 1.0
    return len(left & right) / len(left | right)


def percentile(values, q):
    values = sorted(v for v in values if not math.isnan(v))
    if not values:
        return float("nan")
    rank = max(1, math.ceil(q * len(values)))
    return values[rank - 1]


def summarize_flows(flows):
    completed = [to_float(row.get("fct_s")) for row in flows if row.get("completed") == "true"]
    total_bytes = sum(to_int(row.get("size_bytes")) for row in flows)
    ocs_bytes = sum(to_int(row.get("size_bytes")) for row in flows if row.get("path_type") == "ocs")
    ocs_flows = sum(1 for row in flows if row.get("path_type") == "ocs")
    return {
        "flow_count": len(flows),
        "completed_flow_count": len(completed),
        "incomplete_flow_count": len(flows) - len(completed),
        "completion_ratio": len(completed) / len(flows) if flows else 0.0,
        "avg_fct_s": sum(completed) / len(completed) if completed else float("nan"),
        "p50_fct_s": percentile(completed, 0.50),
        "p90_fct_s": percentile(completed, 0.90),
        "p95_fct_s": percentile(completed, 0.95),
        "p99_fct_s": percentile(completed, 0.99),
        "total_bytes": total_bytes,
        "ocs_flow_hit_rate": ocs_flows / len(flows) if flows else 0.0,
        "ocs_byte_hit_rate": ocs_bytes / total_bytes if total_bytes else 0.0,
    }


def flow_signature(flows):
    return [
        (
            row.get("flow_id"),
            row.get("source_tor"),
            row.get("destination_tor"),
            row.get("size_bytes"),
            row.get("start_time_s"),
        )
        for row in sorted(flows, key=lambda row: to_int(row.get("flow_id")))
    ]


def read_summary_and_flows(stem):
    summary = read_one(RAW_DIR / f"{stem}.csv")
    flows = read_rows(RAW_DIR / f"{stem}-flows.csv")
    return summary, flows


def selected_edges_by_round(scheduling_path):
    rows = read_rows(scheduling_path)
    parsed = []
    union = set()
    for row in rows:
        edges = parse_edges(row.get("selected_edges", ""))
        union |= edges
        parsed.append(
            {
                "start": to_float(row.get("round_start_s")),
                "end": to_float(row.get("round_end_s")),
                "edges": edges,
            }
        )
    return parsed, union


def edge_for_flow(flow):
    a = to_int(flow.get("source_tor"))
    b = to_int(flow.get("destination_tor"))
    return (min(a, b), max(a, b))


def round_edges_for_flow(rounds, start_time):
    for row in rounds:
        if start_time >= row["start"] and start_time < row["end"]:
            return row["edges"]
    return set()


def prepare_candidates(args):
    scenario = args.scenario
    seed = args.seed
    load = args.load
    drain = args.drain
    eps_stem = baseline_stem(scenario, "eps-ecmp", seed, load, drain)
    _, eps_flows = read_summary_and_flows(eps_stem)

    edge_bytes = defaultdict(int)
    for flow in eps_flows:
        edge_bytes[edge_for_flow(flow)] += to_int(flow.get("size_bytes"))
    top_edges = [edge for edge, _ in sorted(edge_bytes.items(), key=lambda item: (-item[1], item[0]))]
    high_impact = set(top_edges[: args.top_edges])

    scheduled_sets = set()
    scheduled_edges = set()
    for scheme in ("ocs-volume", "tl-ocs"):
        sched = RAW_DIR / f"{baseline_stem(scenario, scheme, seed, load, drain)}-scheduling.csv"
        rounds, union = selected_edges_by_round(sched)
        scheduled_edges |= union
        for row in rounds:
            scheduled_sets.add(tuple(sorted(row["edges"])))

    all_rows = []
    all_candidates = sorted(set(all_matchings(range(args.num_tors))), key=lambda item: (len(item), item))
    candidate_count = len(all_candidates)
    selected_rows = []

    for index, matching in enumerate(all_candidates):
        edges = set(matching)
        reasons = []
        if not edges:
            reasons.append("empty")
        if len(edges) == 1:
            reasons.append("all-single-edge")
        if tuple(sorted(edges)) in scheduled_sets:
            reasons.append("baseline-round-edge-set")
        if edges and edges.issubset(high_impact) and len(edges) >= 2:
            reasons.append("high-impact-edge-pool")
        if edges & scheduled_edges and edges.issubset(high_impact | scheduled_edges):
            reasons.append("baseline-plus-high-impact")
        selected = bool(reasons)
        priority = 999
        if "empty" in reasons:
            priority = 0
        elif "baseline-round-edge-set" in reasons:
            priority = 1
        elif "high-impact-edge-pool" in reasons:
            priority = 2
        elif "baseline-plus-high-impact" in reasons:
            priority = 3
        elif "all-single-edge" in reasons:
            priority = 4
        selected_rows.append((priority, -len(edges), index, matching, reasons, selected))

    selected_rows.sort()
    evaluated = set()
    for _, _, _, matching, reasons, selected in selected_rows:
        if selected and len(evaluated) < args.max_candidates:
            evaluated.add(matching)

    for index, matching in enumerate(all_candidates):
        candidate_id = f"cand{index:04d}"
        edges = set(matching)
        reasons = []
        if not edges:
            reasons.append("empty")
        if len(edges) == 1:
            reasons.append("all-single-edge")
        if tuple(sorted(edges)) in scheduled_sets:
            reasons.append("baseline-round-edge-set")
        if edges and edges.issubset(high_impact) and len(edges) >= 2:
            reasons.append("high-impact-edge-pool")
        if edges & scheduled_edges and edges.issubset(high_impact | scheduled_edges):
            reasons.append("baseline-plus-high-impact")
        all_rows.append(
            {
                "scenario": scenario,
                "seed": seed,
                "offered_load_factor": load,
                "drain_label": drain,
                "candidate_id": candidate_id,
                "selected_edges": format_edges(edges),
                "edge_count": len(edges),
                "candidate_count": candidate_count,
                "selected_for_evaluation": matching in evaluated,
                "selection_reason": "|".join(reasons) if reasons else "not-selected",
            }
        )

    fields = [
        "scenario",
        "seed",
        "offered_load_factor",
        "drain_label",
        "candidate_id",
        "selected_edges",
        "edge_count",
        "candidate_count",
        "selected_for_evaluation",
        "selection_reason",
    ]
    write_csv(PLAN_OUT, fields, all_rows)
    print(f"wrote {PLAN_OUT}")
    print(f"candidate_count={candidate_count} evaluated_candidate_count={len(evaluated)}")


def build_baseline_summary(args):
    rows = []
    baseline_data = {}
    for scheme in BASELINE_SCHEMES:
        stem = baseline_stem(args.scenario, scheme, args.seed, args.load, args.drain)
        summary, flows = read_summary_and_flows(stem)
        sched_rounds, union = selected_edges_by_round(RAW_DIR / f"{stem}-scheduling.csv")
        flow_stats = summarize_flows(flows)
        row = {
            "scenario": args.scenario,
            "scheme": scheme,
            "seed": args.seed,
            "offered_load_factor": args.load,
            "drain_label": args.drain,
            "selected_edge_union": format_edges(union),
            "union_edge_count": len(union),
            "avg_round_edge_count": sum(len(r["edges"]) for r in sched_rounds) / len(sched_rounds)
            if sched_rounds
            else 0.0,
            "avg_received_throughput_bps": to_float(summary.get("avg_received_throughput_bps")),
            "eps_avg_link_utilization": to_float(summary.get("eps_avg_link_utilization")),
            "eps_max_link_utilization": to_float(summary.get("eps_max_link_utilization")),
            "ocs_avg_link_utilization": to_float(summary.get("ocs_avg_link_utilization")),
            "ocs_max_link_utilization": to_float(summary.get("ocs_max_link_utilization")),
            **flow_stats,
        }
        rows.append(row)
        baseline_data[scheme] = {
            "summary": summary,
            "flows": flows,
            "rounds": sched_rounds,
            "union": union,
            "row": row,
        }
    return rows, baseline_data


def load_candidate_plan():
    return read_rows(PLAN_OUT)


def build_candidate_rows(args):
    rows = []
    for plan in load_candidate_plan():
        if plan["scenario"] != args.scenario or plan["seed"] != str(args.seed):
            continue
        candidate_id = plan["candidate_id"]
        stem = candidate_stem(args.scenario, args.seed, args.load, args.drain, candidate_id)
        summary_path = RAW_DIR / f"{stem}.csv"
        flow_path = RAW_DIR / f"{stem}-flows.csv"
        evaluated = summary_path.exists() and flow_path.exists()
        row = {
            "scenario": args.scenario,
            "seed": args.seed,
            "offered_load_factor": args.load,
            "drain_label": args.drain,
            "candidate_id": candidate_id,
            "selected_edges": plan["selected_edges"],
            "edge_count": to_int(plan["edge_count"]),
            "candidate_count": to_int(plan["candidate_count"]),
            "evaluated": evaluated,
            "selection_reason": plan["selection_reason"],
            "flow_count": "",
            "completed_flow_count": "",
            "incomplete_flow_count": "",
            "completion_ratio": "",
            "avg_fct_s": "",
            "p90_fct_s": "",
            "p95_fct_s": "",
            "p99_fct_s": "",
            "avg_received_throughput_bps": "",
            "ocs_flow_hit_rate": "",
            "ocs_byte_hit_rate": "",
            "eps_avg_link_utilization": "",
            "eps_max_link_utilization": "",
            "ocs_avg_link_utilization": "",
            "ocs_max_link_utilization": "",
        }
        if evaluated:
            summary, flows = read_summary_and_flows(stem)
            stats = summarize_flows(flows)
            row.update(stats)
            row["avg_received_throughput_bps"] = to_float(summary.get("avg_received_throughput_bps"))
            row["eps_avg_link_utilization"] = to_float(summary.get("eps_avg_link_utilization"))
            row["eps_max_link_utilization"] = to_float(summary.get("eps_max_link_utilization"))
            row["ocs_avg_link_utilization"] = to_float(summary.get("ocs_avg_link_utilization"))
            row["ocs_max_link_utilization"] = to_float(summary.get("ocs_max_link_utilization"))
        rows.append(row)
    return rows


def best_candidate(rows, objective):
    evaluated = [row for row in rows if row["evaluated"]]
    if objective == "avg_fct":
        return min(
            evaluated,
            key=lambda row: (
                to_float(row["avg_fct_s"]),
                to_float(row["p95_fct_s"]),
                -to_float(row["completion_ratio"]),
                -to_float(row["avg_received_throughput_bps"]),
                row["candidate_id"],
            ),
        )
    return min(
        evaluated,
        key=lambda row: (
            to_float(row["p95_fct_s"]),
            to_float(row["avg_fct_s"]),
            -to_float(row["completion_ratio"]),
            -to_float(row["avg_received_throughput_bps"]),
            row["candidate_id"],
        ),
    )


def build_oracle_summary(args, candidate_rows, baseline_rows, baseline_data):
    avg_best = best_candidate(candidate_rows, "avg_fct")
    p95_best = best_candidate(candidate_rows, "p95_fct")
    avg_edges = parse_edges(avg_best["selected_edges"])
    rows = []
    evaluated_count = sum(1 for row in candidate_rows if row["evaluated"])
    candidate_count = max(to_int(row["candidate_count"]) for row in candidate_rows)
    for row in baseline_rows:
        scheme = row["scheme"]
        union = baseline_data[scheme]["union"]
        rounds = baseline_data[scheme]["rounds"]
        round_jaccards = [jaccard(r["edges"], avg_edges) for r in rounds]
        rows.append(
            {
                "scenario": args.scenario,
                "seed": args.seed,
                "offered_load_factor": args.load,
                "drain_label": args.drain,
                "row_type": "baseline",
                "scheme": scheme,
                "candidate_id": "",
                "selected_edges": row["selected_edge_union"],
                "fct_oracle_edges": avg_best["selected_edges"],
                "avg_fct_oracle_candidate_id": avg_best["candidate_id"],
                "p95_fct_oracle_candidate_id": p95_best["candidate_id"],
                "candidate_count": candidate_count,
                "evaluated_candidate_count": evaluated_count,
                "skipped_candidate_count": candidate_count - evaluated_count,
                "union_jaccard_to_fct_oracle": jaccard(union, avg_edges),
                "round_avg_jaccard_to_fct_oracle": sum(round_jaccards) / len(round_jaccards)
                if round_jaccards
                else float("nan"),
                "edges_minus_fct_oracle": format_edges(union - avg_edges),
                "fct_oracle_edges_minus_scheme": format_edges(avg_edges - union),
                "avg_fct_s": row["avg_fct_s"],
                "p95_fct_s": row["p95_fct_s"],
                "completion_ratio": row["completion_ratio"],
                "avg_received_throughput_bps": row["avg_received_throughput_bps"],
                "ocs_byte_hit_rate": row["ocs_byte_hit_rate"],
                "eps_avg_link_utilization": row["eps_avg_link_utilization"],
                "eps_max_link_utilization": row["eps_max_link_utilization"],
                "ocs_avg_link_utilization": row["ocs_avg_link_utilization"],
                "ocs_max_link_utilization": row["ocs_max_link_utilization"],
            }
        )
    rows.append(
        {
            "scenario": args.scenario,
            "seed": args.seed,
            "offered_load_factor": args.load,
            "drain_label": args.drain,
            "row_type": "fct-oracle-avg",
            "scheme": "fct-oracle",
            "candidate_id": avg_best["candidate_id"],
            "selected_edges": avg_best["selected_edges"],
            "fct_oracle_edges": avg_best["selected_edges"],
            "avg_fct_oracle_candidate_id": avg_best["candidate_id"],
            "p95_fct_oracle_candidate_id": p95_best["candidate_id"],
            "candidate_count": candidate_count,
            "evaluated_candidate_count": evaluated_count,
            "skipped_candidate_count": candidate_count - evaluated_count,
            "union_jaccard_to_fct_oracle": 1.0,
            "round_avg_jaccard_to_fct_oracle": 1.0,
            "edges_minus_fct_oracle": "",
            "fct_oracle_edges_minus_scheme": "",
            "avg_fct_s": avg_best["avg_fct_s"],
            "p95_fct_s": avg_best["p95_fct_s"],
            "completion_ratio": avg_best["completion_ratio"],
            "avg_received_throughput_bps": avg_best["avg_received_throughput_bps"],
            "ocs_byte_hit_rate": avg_best["ocs_byte_hit_rate"],
            "eps_avg_link_utilization": avg_best["eps_avg_link_utilization"],
            "eps_max_link_utilization": avg_best["eps_max_link_utilization"],
            "ocs_avg_link_utilization": avg_best["ocs_avg_link_utilization"],
            "ocs_max_link_utilization": avg_best["ocs_max_link_utilization"],
        }
    )
    return rows, avg_best, p95_best


def time_bucket(start):
    if start < 0.005:
        return "before-first-schedule"
    if start < 0.025:
        return "early-period"
    return "late-period"


def size_bucket(size, median_size):
    return "large" if size >= median_size else "small"


def build_attribution(args, baseline_data, fct_edges):
    tl_rounds = baseline_data["tl-ocs"]["rounds"]
    volume_rounds = baseline_data["ocs-volume"]["rounds"]
    all_sizes = [to_int(row.get("size_bytes")) for row in baseline_data["eps-ecmp"]["flows"]]
    median = percentile(all_sizes, 0.5)
    rows = []
    for scheme in BASELINE_SCHEMES:
        for flow in baseline_data[scheme]["flows"]:
            start = to_float(flow.get("start_time_s"))
            edge = edge_for_flow(flow)
            tl_edges = round_edges_for_flow(tl_rounds, start)
            volume_edges = round_edges_for_flow(volume_rounds, start)
            is_tl = edge in tl_edges
            is_volume = edge in volume_edges
            is_fct = edge in fct_edges
            path_type = flow.get("path_type", "")
            selected_by_scheme = (scheme == "tl-ocs" and is_tl) or (
                scheme == "ocs-volume" and is_volume
            )
            rows.append(
                {
                    "scenario": args.scenario,
                    "scheme": scheme,
                    "seed": args.seed,
                    "offered_load_factor": args.load,
                    "drain_label": args.drain,
                    "flow_id": flow.get("flow_id"),
                    "start_time_s": start,
                    "round_index": int(start / 0.005) if start >= 0.005 else 0,
                    "src_tor": flow.get("source_tor"),
                    "dst_tor": flow.get("destination_tor"),
                    "edge_pair": f"{edge[0]}-{edge[1]}",
                    "size_bytes": to_int(flow.get("size_bytes")),
                    "path_type": path_type,
                    "completed": flow.get("completed"),
                    "fct_s": to_float(flow.get("fct_s")),
                    "is_on_tl_selected_edge": is_tl,
                    "is_on_volume_selected_edge": is_volume,
                    "is_on_fct_oracle_edge": is_fct,
                    "is_common_tl_volume_edge": is_tl and is_volume,
                    "is_tl_only_edge": is_tl and not is_volume,
                    "is_volume_only_edge": is_volume and not is_tl,
                    "is_fct_oracle_only_edge": is_fct and not is_tl and not is_volume,
                    "is_selected_but_eps_fallback": selected_by_scheme and path_type == "eps",
                    "is_selected_and_ocs_admitted": selected_by_scheme and path_type == "ocs",
                    "size_bucket": size_bucket(to_int(flow.get("size_bytes")), median),
                    "time_bucket": time_bucket(start),
                }
            )
    return rows


def summarize_category(rows, category_type, category_name, predicate):
    selected = [row for row in rows if predicate(row)]
    completed = [to_float(row["fct_s"]) for row in selected if row["completed"] == "true"]
    total_bytes = sum(to_int(row["size_bytes"]) for row in selected)
    fct_sum = sum(completed)
    return {
        "category_type": category_type,
        "category": category_name,
        "flow_count": len(selected),
        "total_bytes": total_bytes,
        "completed_flow_count": len(completed),
        "completion_ratio": len(completed) / len(selected) if selected else 0.0,
        "avg_fct_s": sum(completed) / len(completed) if completed else float("nan"),
        "p50_fct_s": percentile(completed, 0.50),
        "p90_fct_s": percentile(completed, 0.90),
        "p95_fct_s": percentile(completed, 0.95),
        "avg_size_bytes": total_bytes / len(selected) if selected else 0.0,
        "ocs_admitted_count": sum(1 for row in selected if row["path_type"] == "ocs"),
        "eps_fallback_count": sum(1 for row in selected if row["path_type"] == "eps"),
        "contribution_to_total_fct_sum": fct_sum,
    }


def build_attribution_summary(attr_rows):
    rows = []
    predicates = [
        ("edge_relation", "common_tl_volume_edge", lambda r: r["is_common_tl_volume_edge"]),
        ("edge_relation", "tl_only_edge", lambda r: r["is_tl_only_edge"]),
        ("edge_relation", "volume_only_edge", lambda r: r["is_volume_only_edge"]),
        ("edge_relation", "fct_oracle_only_edge", lambda r: r["is_fct_oracle_only_edge"]),
        (
            "edge_relation",
            "selected_by_none",
            lambda r: not r["is_on_tl_selected_edge"]
            and not r["is_on_volume_selected_edge"]
            and not r["is_on_fct_oracle_edge"],
        ),
        ("admission", "ocs_admitted", lambda r: r["path_type"] == "ocs"),
        ("admission", "selected_but_eps_fallback", lambda r: r["is_selected_but_eps_fallback"]),
        ("admission", "selected_and_ocs_admitted", lambda r: r["is_selected_and_ocs_admitted"]),
        ("size_bucket", "small", lambda r: r["size_bucket"] == "small"),
        ("size_bucket", "large", lambda r: r["size_bucket"] == "large"),
        ("time_bucket", "before-first-schedule", lambda r: r["time_bucket"] == "before-first-schedule"),
        ("time_bucket", "early-period", lambda r: r["time_bucket"] == "early-period"),
        ("time_bucket", "late-period", lambda r: r["time_bucket"] == "late-period"),
    ]
    for scheme in BASELINE_SCHEMES:
        scheme_rows = [row for row in attr_rows if row["scheme"] == scheme]
        for category_type, category_name, predicate in predicates:
            row = summarize_category(scheme_rows, category_type, category_name, predicate)
            row["scheme"] = scheme
            rows.append(row)
    return rows


def build_explanation(baseline_data, attr_summary, oracle_rows):
    by_scheme = {row["scheme"]: row for row in oracle_rows if row["row_type"] == "baseline"}
    explanation = []
    if "tl-ocs" in by_scheme and "ocs-volume" in by_scheme:
        tl = by_scheme["tl-ocs"]
        volume = by_scheme["ocs-volume"]
        explanation.append(
            {
                "finding": "tl_minus_volume_avg_fct_s",
                "value": to_float(tl["avg_fct_s"]) - to_float(volume["avg_fct_s"]),
                "interpretation": "positive means TL has worse average FCT than Volume",
            }
        )
        explanation.append(
            {
                "finding": "tl_minus_volume_p95_fct_s",
                "value": to_float(tl["p95_fct_s"]) - to_float(volume["p95_fct_s"]),
                "interpretation": "positive means TL has worse tail FCT than Volume",
            }
        )
        explanation.append(
            {
                "finding": "fct_oracle_closer_to",
                "value": "volume"
                if to_float(volume["union_jaccard_to_fct_oracle"])
                >= to_float(tl["union_jaccard_to_fct_oracle"])
                else "tl",
                "interpretation": "computed from selected-edge union Jaccard to avg-FCT oracle",
            }
        )
    indexed = {(row["scheme"], row["category"]): row for row in attr_summary}
    for category in ["tl_only_edge", "volume_only_edge", "selected_but_eps_fallback", "ocs_admitted"]:
        tl_row = indexed.get(("tl-ocs", category), {})
        volume_row = indexed.get(("ocs-volume", category), {})
        explanation.append(
            {
                "finding": f"category_{category}_tl_vs_volume_avg_fct_s",
                "value": to_float(tl_row.get("avg_fct_s")) - to_float(volume_row.get("avg_fct_s")),
                "interpretation": "category-level TL average FCT minus Volume average FCT",
            }
        )
    return explanation


def aggregate(args):
    baseline_rows, baseline_data = build_baseline_summary(args)
    candidate_rows = build_candidate_rows(args)
    oracle_rows, avg_best, p95_best = build_oracle_summary(
        args, candidate_rows, baseline_rows, baseline_data
    )
    fct_edges = parse_edges(avg_best["selected_edges"])
    attr_rows = build_attribution(args, baseline_data, fct_edges)
    attr_summary = build_attribution_summary(attr_rows)
    explanation = build_explanation(baseline_data, attr_summary, oracle_rows)

    candidate_fields = [
        "scenario",
        "seed",
        "offered_load_factor",
        "drain_label",
        "candidate_id",
        "selected_edges",
        "edge_count",
        "candidate_count",
        "evaluated",
        "selection_reason",
        "flow_count",
        "completed_flow_count",
        "incomplete_flow_count",
        "completion_ratio",
        "avg_fct_s",
        "p90_fct_s",
        "p95_fct_s",
        "p99_fct_s",
        "avg_received_throughput_bps",
        "ocs_flow_hit_rate",
        "ocs_byte_hit_rate",
        "eps_avg_link_utilization",
        "eps_max_link_utilization",
        "ocs_avg_link_utilization",
        "ocs_max_link_utilization",
    ]
    baseline_fields = [
        "scenario",
        "scheme",
        "seed",
        "offered_load_factor",
        "drain_label",
        "selected_edge_union",
        "union_edge_count",
        "avg_round_edge_count",
        "flow_count",
        "completed_flow_count",
        "incomplete_flow_count",
        "completion_ratio",
        "avg_fct_s",
        "p90_fct_s",
        "p95_fct_s",
        "p99_fct_s",
        "avg_received_throughput_bps",
        "ocs_flow_hit_rate",
        "ocs_byte_hit_rate",
        "eps_avg_link_utilization",
        "eps_max_link_utilization",
        "ocs_avg_link_utilization",
        "ocs_max_link_utilization",
    ]
    oracle_fields = [
        "scenario",
        "seed",
        "offered_load_factor",
        "drain_label",
        "row_type",
        "scheme",
        "candidate_id",
        "selected_edges",
        "fct_oracle_edges",
        "avg_fct_oracle_candidate_id",
        "p95_fct_oracle_candidate_id",
        "candidate_count",
        "evaluated_candidate_count",
        "skipped_candidate_count",
        "union_jaccard_to_fct_oracle",
        "round_avg_jaccard_to_fct_oracle",
        "edges_minus_fct_oracle",
        "fct_oracle_edges_minus_scheme",
        "avg_fct_s",
        "p95_fct_s",
        "completion_ratio",
        "avg_received_throughput_bps",
        "ocs_byte_hit_rate",
        "eps_avg_link_utilization",
        "eps_max_link_utilization",
        "ocs_avg_link_utilization",
        "ocs_max_link_utilization",
    ]
    attr_flow_fields = [
        "scenario",
        "scheme",
        "seed",
        "offered_load_factor",
        "drain_label",
        "flow_id",
        "start_time_s",
        "round_index",
        "src_tor",
        "dst_tor",
        "edge_pair",
        "size_bytes",
        "path_type",
        "completed",
        "fct_s",
        "is_on_tl_selected_edge",
        "is_on_volume_selected_edge",
        "is_on_fct_oracle_edge",
        "is_common_tl_volume_edge",
        "is_tl_only_edge",
        "is_volume_only_edge",
        "is_fct_oracle_only_edge",
        "is_selected_but_eps_fallback",
        "is_selected_and_ocs_admitted",
        "size_bucket",
        "time_bucket",
    ]
    attr_summary_fields = [
        "scheme",
        "category_type",
        "category",
        "flow_count",
        "total_bytes",
        "completed_flow_count",
        "completion_ratio",
        "avg_fct_s",
        "p50_fct_s",
        "p90_fct_s",
        "p95_fct_s",
        "avg_size_bytes",
        "ocs_admitted_count",
        "eps_fallback_count",
        "contribution_to_total_fct_sum",
    ]
    explanation_fields = ["finding", "value", "interpretation"]

    write_csv(CANDIDATES_OUT, candidate_fields, candidate_rows)
    write_csv(BASELINE_OUT, baseline_fields, baseline_rows)
    write_csv(ORACLE_OUT, oracle_fields, oracle_rows)
    write_csv(ATTR_FLOWS_OUT, attr_flow_fields, attr_rows)
    write_csv(ATTR_SUMMARY_OUT, attr_summary_fields, attr_summary)
    write_csv(ATTR_EXPLANATION_OUT, explanation_fields, explanation)

    errors = []
    warnings = []
    signatures = []
    for scheme in BASELINE_SCHEMES:
        signatures.append((scheme, flow_signature(baseline_data[scheme]["flows"])))
    best_stem = candidate_stem(args.scenario, args.seed, args.load, args.drain, avg_best["candidate_id"])
    _, best_flows = read_summary_and_flows(best_stem)
    signatures.append(("fct-oracle", flow_signature(best_flows)))
    reference = signatures[0][1]
    for name, signature in signatures[1:]:
        if signature != reference:
            errors.append(f"flow sequence mismatch for {name}")
    if not any(row["evaluated"] for row in candidate_rows):
        errors.append("no FCT oracle candidates were evaluated")
    if "future" in ",".join(oracle_fields + baseline_fields + candidate_fields):
        errors.append("R6 processed schema contains legacy demand-oracle fields")
    quality = [
        {
            "status": "failed" if errors else "passed",
            "candidate_count": max(to_int(row["candidate_count"]) for row in candidate_rows),
            "evaluated_candidate_count": sum(1 for row in candidate_rows if row["evaluated"]),
            "skipped_candidate_count": sum(1 for row in candidate_rows if not row["evaluated"]),
            "baseline_summary_count": len(baseline_rows),
            "attribution_flow_count": len(attr_rows),
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
            "candidate_count",
            "evaluated_candidate_count",
            "skipped_candidate_count",
            "baseline_summary_count",
            "attribution_flow_count",
            "error_count",
            "warning_count",
            "errors",
            "warnings",
        ],
        quality,
    )
    print(f"wrote {CANDIDATES_OUT}")
    print(f"wrote {ORACLE_OUT}")
    print(f"wrote {ATTR_SUMMARY_OUT}")
    print(f"quality status: {quality[0]['status']}")


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    prepare = sub.add_parser("prepare-candidates")
    prepare.add_argument("--scenario", default="cross-community-distractor-replay")
    prepare.add_argument("--load", type=float, default=0.5)
    prepare.add_argument("--seed", type=int, default=1401)
    prepare.add_argument("--drain", default="D3")
    prepare.add_argument("--num-tors", type=int, default=8)
    prepare.add_argument("--top-edges", type=int, default=8)
    prepare.add_argument("--max-candidates", type=int, default=80)

    aggregate_parser = sub.add_parser("aggregate")
    aggregate_parser.add_argument("--scenario", default="cross-community-distractor-replay")
    aggregate_parser.add_argument("--load", type=float, default=0.5)
    aggregate_parser.add_argument("--seed", type=int, default=1401)
    aggregate_parser.add_argument("--drain", default="D3")

    args = parser.parse_args()
    if args.command == "prepare-candidates":
        prepare_candidates(args)
    else:
        aggregate(args)


if __name__ == "__main__":
    main()
