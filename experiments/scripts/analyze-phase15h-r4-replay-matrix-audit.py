#!/usr/bin/env python3
"""Audit Phase 15H-R4 matrix-replay workloads before scheme comparison."""

import argparse
import csv
import math
import re
from pathlib import Path

RAW_DIR = Path("results/raw")
PROCESSED_DIR = Path("results/processed")
AUDIT_OUT = PROCESSED_DIR / "phase15h-r4-replay-matrix-audit.csv"

NAME_RE = re.compile(
    r"^phase15h-r4-(?P<scenario>.+)-eps-ecmp-seed(?P<seed>\d+)-load(?P<load>[0-9]+p[0-9]+)-flows$"
)

TOP_K = 8
NUM_TORS = 8
OBSERVER_WINDOW_S = 0.001
OCS_PERIOD_S = 0.005
STOP_TIME_S = 0.05
ETA = 1.0
ALPHA = 0.5
THETA_F = 0.0
PORTS = 1
FUTURE_REPLAY_SCALE = 4.0

FIELDS = [
    "target_matrix_name",
    "scenario",
    "load",
    "seed",
    "period_index",
    "target_top_k_edges",
    "actual_observed_w_prev_top_k_edges",
    "actual_future_d_future_top_k_edges",
    "corr_target_matrix_w_prev",
    "corr_target_matrix_d_future",
    "corr_w_prev_d_future",
    "top_k_overlap_target_w_prev",
    "top_k_overlap_target_d_future",
    "top_k_overlap_w_prev_d_future",
    "normalized_matrix_error_target_w_prev",
    "normalized_matrix_error_target_d_future",
    "demand_drift_ratio",
    "volume_selected_edges_on_target",
    "tl_selected_edges_on_target",
    "volume_selected_edges_on_actual_w_prev",
    "tl_selected_edges_on_actual_w_prev",
    "volume_tl_jaccard_on_target",
    "volume_tl_jaccard_on_actual_w_prev",
    "tl_future_demand_coverage",
    "volume_future_demand_coverage",
    "oracle_future_demand_coverage",
    "gate_pass",
    "gate_failure_reason",
]


def zero_matrix(n=NUM_TORS):
    return [[0.0 for _ in range(n)] for _ in range(n)]


def add_edge(matrix, i, j, value):
    matrix[i][j] += value
    matrix[j][i] += value


def add_directed(matrix, i, j, value):
    matrix[i][j] += value


def clone(matrix):
    return [row[:] for row in matrix]


def scale_and_add(target, source, scale):
    for i in range(len(target)):
        for j in range(len(target)):
            target[i][j] += source[i][j] * scale


def threshold(matrix, theta_f):
    filtered = clone(matrix)
    n = len(filtered)
    for i in range(n):
        for j in range(i + 1, n):
            if filtered[i][j] < theta_f:
                filtered[i][j] = 0.0
                filtered[j][i] = 0.0
    return filtered


def degrees(matrix):
    return [sum(row) for row in matrix]


def total_traffic(matrix):
    return sum(sum(row) for row in matrix) * 0.5


def expected_matrix(matrix):
    n = len(matrix)
    degree = degrees(matrix)
    total = total_traffic(matrix)
    expected = zero_matrix(n)
    if total <= 0.0:
        return expected
    for i in range(n):
        for j in range(i + 1, n):
            value = degree[i] * degree[j] / (2.0 * total)
            expected[i][j] = value
            expected[j][i] = value
    return expected


def modularity_gain(matrix, eta):
    n = len(matrix)
    expected = expected_matrix(matrix)
    gain = zero_matrix(n)
    for i in range(n):
        for j in range(i + 1, n):
            value = matrix[i][j] - eta * expected[i][j]
            gain[i][j] = value
            gain[j][i] = value
    return gain


def normalize_labels(labels):
    seen = []
    out = []
    for label in labels:
        if label not in seen:
            seen.append(label)
        out.append(seen.index(label))
    return out


def pair_gain(matrix, a, b):
    return matrix[min(a, b)][max(a, b)]


def move_gain(matrix, labels, node, target):
    old = labels[node]
    removed = 0.0
    added = 0.0
    for other in range(len(matrix)):
        if other == node:
            continue
        if labels[other] == old:
            removed += pair_gain(matrix, node, other)
        if labels[other] == target:
            added += pair_gain(matrix, node, other)
    return added - removed


def run_local_moving(matrix, max_passes=4, min_gain=1e-12):
    labels = list(range(len(matrix)))
    for _ in range(max_passes):
        moved = False
        for node in range(len(matrix)):
            old = labels[node]
            candidates = [old]
            for neighbor in range(len(matrix)):
                if neighbor == node or pair_gain(matrix, node, neighbor) == 0.0:
                    continue
                label = labels[neighbor]
                if label not in candidates:
                    candidates.append(label)
            candidates = sorted(candidates)
            if labels.count(old) > 1:
                candidates.append(max(labels) + 1 if labels else 0)
            best = old
            best_gain = 0.0
            for candidate in candidates:
                if candidate == old:
                    continue
                gain = move_gain(matrix, labels, node, candidate)
                if gain > min_gain and (
                    best == old
                    or gain > best_gain + min_gain
                    or (abs(gain - best_gain) <= min_gain and candidate < best)
                ):
                    best = candidate
                    best_gain = gain
            if best != old:
                labels[node] = best
                moved = True
        if not moved:
            break
    return normalize_labels(labels)


def aggregate_matrix(matrix, labels):
    count = max(labels) + 1 if labels else 0
    aggregated = zero_matrix(count)
    for i in range(len(matrix)):
        for j in range(i + 1, len(matrix)):
            if labels[i] == labels[j]:
                continue
            value = matrix[i][j]
            aggregated[labels[i]][labels[j]] += value
            aggregated[labels[j]][labels[i]] += value
    return aggregated


def expand_labels(parent_labels, local_labels):
    return [local_labels[parent] for parent in parent_labels]


def detect_communities(matrix, max_levels=4):
    labels = list(range(len(matrix)))
    current = clone(matrix)
    for _ in range(max_levels):
        local_labels = run_local_moving(current)
        labels = normalize_labels(expand_labels(labels, local_labels))
        local_count = max(local_labels) + 1 if local_labels else 0
        if local_count == len(current):
            break
        current = aggregate_matrix(current, local_labels)
    return normalize_labels(labels)


def top_edges(matrix, k=TOP_K, positive_only=False):
    edges = []
    for i in range(len(matrix)):
        for j in range(i + 1, len(matrix)):
            value = matrix[i][j]
            if positive_only and value <= 0.0:
                continue
            edges.append((i, j, value))
    edges.sort(key=lambda edge: (-edge[2], edge[0], edge[1]))
    return edges[:k]


def edge_set(edges):
    return {(i, j) for i, j, _ in edges}


def select_edges(scored_edges, n, ports):
    selected = []
    degree = [0] * n
    for i, j, score in sorted(scored_edges, key=lambda edge: (-edge[2], edge[0], edge[1])):
        if score <= 0.0:
            continue
        if degree[i] >= ports or degree[j] >= ports:
            continue
        degree[i] += 1
        degree[j] += 1
        selected.append((i, j, score))
    return selected


def select_volume(matrix, ports=PORTS):
    return select_edges(top_edges(matrix, k=len(matrix) * len(matrix), positive_only=True), len(matrix), ports)


def tl_g_edges(gain, labels, alpha):
    edges = []
    for i in range(len(gain)):
        for j in range(i + 1, len(gain)):
            base = max(gain[i][j], 0.0)
            factor = 1.0 if labels[i] == labels[j] else alpha
            score = base * factor
            if score > 0.0:
                edges.append((i, j, score))
    edges.sort(key=lambda edge: (-edge[2], edge[0], edge[1]))
    return edges


def select_tl(matrix, ports=PORTS):
    a = threshold(matrix, THETA_F)
    gain = modularity_gain(a, ETA)
    labels = detect_communities(gain)
    return select_edges(tl_g_edges(gain, labels, ALPHA), len(matrix), ports)


def coverage(matrix, selected):
    total = total_traffic(matrix)
    if total <= 0.0:
        return 0.0
    return sum(matrix[i][j] for i, j, _ in selected) / total


def jaccard(left, right):
    left_set = edge_set(left)
    right_set = edge_set(right)
    if not left_set and not right_set:
        return 1.0
    union = left_set | right_set
    return len(left_set & right_set) / len(union) if union else 0.0


def fmt_edges(edges):
    return ";".join(f"{i}-{j}:{value:.3f}" for i, j, value in edges)


def pearson(left, right):
    values_left = []
    values_right = []
    for i in range(len(left)):
        for j in range(i + 1, len(left)):
            values_left.append(left[i][j])
            values_right.append(right[i][j])
    if not values_left:
        return 0.0
    mean_left = sum(values_left) / len(values_left)
    mean_right = sum(values_right) / len(values_right)
    numerator = sum((a - mean_left) * (b - mean_right) for a, b in zip(values_left, values_right))
    denom_left = math.sqrt(sum((a - mean_left) ** 2 for a in values_left))
    denom_right = math.sqrt(sum((b - mean_right) ** 2 for b in values_right))
    if denom_left == 0.0 or denom_right == 0.0:
        return 1.0 if all(abs(a - b) < 1e-9 for a, b in zip(values_left, values_right)) else 0.0
    return numerator / (denom_left * denom_right)


def topk_overlap(left, right, k=TOP_K):
    left_set = edge_set(top_edges(left, k=k, positive_only=True))
    right_set = edge_set(top_edges(right, k=k, positive_only=True))
    if not left_set and not right_set:
        return 1.0
    union = left_set | right_set
    return len(left_set & right_set) / len(union) if union else 0.0


def normalized_error(target, actual):
    target_total = total_traffic(target)
    actual_total = total_traffic(actual)
    if target_total <= 0.0:
        return 0.0 if actual_total <= 0.0 else 1.0
    scale = actual_total / target_total
    numerator = 0.0
    denominator = 0.0
    for i in range(len(target)):
        for j in range(i + 1, len(target)):
            expected = target[i][j] * scale
            numerator += abs(actual[i][j] - expected)
            denominator += max(expected, 1.0)
    return numerator / denominator if denominator > 0.0 else 0.0


def drift_ratio(prev, future):
    numerator = 0.0
    denominator = 0.0
    for i in range(len(prev)):
        for j in range(i + 1, len(prev)):
            numerator += abs(future[i][j] - prev[i][j])
            denominator += max(prev[i][j], 1.0)
    return numerator / denominator if denominator > 0.0 else 0.0


def high_degree_history():
    matrix = zero_matrix()
    for worker in [1, 2, 3, 4, 5, 6]:
        add_edge(matrix, 0, worker, 95.0)
    for edge in [(1, 2), (2, 3), (4, 5), (5, 6)]:
        add_edge(matrix, *edge, 88.0)
    for edge in [(1, 3), (4, 6)]:
        add_edge(matrix, *edge, 72.0)
    add_edge(matrix, 6, 7, 40.0)
    return matrix


def high_degree_future():
    matrix = zero_matrix()
    for edge in [(0, 1), (0, 2), (0, 4), (0, 5), (0, 6)]:
        add_edge(matrix, *edge, 33.25)
    add_edge(matrix, 0, 3, 68.25)
    for edge in [(1, 2), (4, 5), (5, 6)]:
        add_edge(matrix, *edge, 156.0)
    add_edge(matrix, 2, 3, 106.0)
    add_edge(matrix, 1, 3, 72.0)
    add_edge(matrix, 4, 6, 72.0)
    add_edge(matrix, 6, 7, 118.0)
    return matrix


def cross_history():
    matrix = zero_matrix()
    for edge in [(0, 1), (1, 2), (4, 5), (5, 6), (2, 3), (6, 7)]:
        add_edge(matrix, *edge, 105.0)
    add_edge(matrix, 3, 4, 132.0)
    add_edge(matrix, 0, 4, 58.0)
    add_edge(matrix, 1, 5, 52.0)
    add_edge(matrix, 2, 6, 50.0)
    return matrix


def cross_future():
    matrix = zero_matrix()
    add_edge(matrix, 0, 1, 145.0)
    add_edge(matrix, 1, 2, 145.0)
    add_edge(matrix, 4, 5, 105.0)
    add_edge(matrix, 5, 6, 57.75)
    add_edge(matrix, 2, 3, 105.0)
    add_edge(matrix, 6, 7, 180.0)
    add_edge(matrix, 3, 4, 59.4)
    add_edge(matrix, 0, 4, 58.0)
    add_edge(matrix, 1, 5, 52.0)
    add_edge(matrix, 2, 6, 50.0)
    return matrix


def target_matrices(scenario):
    if scenario == "high-degree-aggregator-bias-replay":
        history = high_degree_history()
        future = high_degree_future()
    elif scenario == "cross-community-distractor-replay":
        history = cross_history()
        future = cross_future()
    else:
        raise ValueError(f"unsupported R4 replay scenario {scenario}")
    combined = zero_matrix()
    scale_and_add(combined, future, FUTURE_REPLAY_SCALE)
    scale_and_add(combined, history, 1.0)
    return history, combined


def parse_name(path):
    match = NAME_RE.match(path.stem)
    if not match:
        return None
    parsed = match.groupdict()
    parsed["seed"] = int(parsed["seed"])
    parsed["load"] = float(parsed["load"].replace("p", "."))
    return parsed


def read_flows(path):
    rows = []
    with path.open(newline="") as stream:
        for row in csv.DictReader(stream):
            rows.append(
                {
                    "source": int(row["source_tor"]),
                    "destination": int(row["destination_tor"]),
                    "size": int(float(row["size_bytes"])),
                    "start": float(row["start_time_s"]),
                }
            )
    return rows


def matrix_from_flows(flows, start, end):
    matrix = zero_matrix()
    for flow in flows:
        if start <= flow["start"] < end:
            add_directed(matrix, flow["source"], flow["destination"], flow["size"])
    undirected = zero_matrix()
    for i in range(NUM_TORS):
        for j in range(i + 1, NUM_TORS):
            value = matrix[i][j] + matrix[j][i]
            undirected[i][j] = value
            undirected[j][i] = value
    return undirected


def gate_reasons(corr_target_future, overlap_target_future, error_target_future, jaccard_actual, tl_cov, volume_cov):
    reasons = []
    if corr_target_future < 0.95 and overlap_target_future < 0.8:
        reasons.append("target-vs-D_future correlation/top-k overlap below threshold")
    if error_target_future > 0.10:
        reasons.append("target-vs-D_future normalized error above 0.10")
    if jaccard_actual >= 0.999999:
        reasons.append("Volume/TL selectedEdges did not diverge on actual W_prev")
    if tl_cov <= volume_cov:
        reasons.append("TL future-demand coverage did not exceed Volume")
    return reasons


def analyze_file(path):
    parsed = parse_name(path)
    if parsed is None:
        return []
    history_target, future_target = target_matrices(parsed["scenario"])
    flows = read_flows(path)
    rows = []
    period_index = 1
    while period_index * OCS_PERIOD_S < STOP_TIME_S - 1e-12:
        round_start = period_index * OCS_PERIOD_S
        round_end = min(round_start + OCS_PERIOD_S, STOP_TIME_S)
        observed = matrix_from_flows(flows, round_start - OBSERVER_WINDOW_S, round_start)
        future = matrix_from_flows(flows, round_start, round_end)

        volume_target = select_volume(history_target)
        tl_target = select_tl(history_target)
        volume_actual = select_volume(observed)
        tl_actual = select_tl(observed)
        oracle_actual = select_volume(future)

        corr_target_w = pearson(history_target, observed)
        corr_target_future = pearson(future_target, future)
        corr_w_future = pearson(observed, future)
        overlap_target_w = topk_overlap(history_target, observed)
        overlap_target_future = topk_overlap(future_target, future)
        overlap_w_future = topk_overlap(observed, future)
        error_target_w = normalized_error(history_target, observed)
        error_target_future = normalized_error(future_target, future)
        jaccard_target = jaccard(volume_target, tl_target)
        jaccard_actual = jaccard(volume_actual, tl_actual)
        volume_cov = coverage(future, volume_actual)
        tl_cov = coverage(future, tl_actual)
        oracle_cov = coverage(future, oracle_actual)
        reasons = gate_reasons(
            corr_target_future,
            overlap_target_future,
            error_target_future,
            jaccard_actual,
            tl_cov,
            volume_cov,
        )

        rows.append(
            {
                "target_matrix_name": f"{parsed['scenario']}:history-to-combined-future",
                "scenario": parsed["scenario"],
                "load": parsed["load"],
                "seed": parsed["seed"],
                "period_index": period_index,
                "target_top_k_edges": fmt_edges(top_edges(future_target, positive_only=True)),
                "actual_observed_w_prev_top_k_edges": fmt_edges(top_edges(observed, positive_only=True)),
                "actual_future_d_future_top_k_edges": fmt_edges(top_edges(future, positive_only=True)),
                "corr_target_matrix_w_prev": corr_target_w,
                "corr_target_matrix_d_future": corr_target_future,
                "corr_w_prev_d_future": corr_w_future,
                "top_k_overlap_target_w_prev": overlap_target_w,
                "top_k_overlap_target_d_future": overlap_target_future,
                "top_k_overlap_w_prev_d_future": overlap_w_future,
                "normalized_matrix_error_target_w_prev": error_target_w,
                "normalized_matrix_error_target_d_future": error_target_future,
                "demand_drift_ratio": drift_ratio(observed, future),
                "volume_selected_edges_on_target": fmt_edges(volume_target),
                "tl_selected_edges_on_target": fmt_edges(tl_target),
                "volume_selected_edges_on_actual_w_prev": fmt_edges(volume_actual),
                "tl_selected_edges_on_actual_w_prev": fmt_edges(tl_actual),
                "volume_tl_jaccard_on_target": jaccard_target,
                "volume_tl_jaccard_on_actual_w_prev": jaccard_actual,
                "tl_future_demand_coverage": tl_cov,
                "volume_future_demand_coverage": volume_cov,
                "oracle_future_demand_coverage": oracle_cov,
                "gate_pass": not reasons,
                "gate_failure_reason": " | ".join(reasons),
            }
        )
        period_index += 1
    return rows


def fmt(value):
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, float):
        if math.isnan(value):
            return ""
        return f"{value:.12g}"
    return str(value)


def write_csv(path, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS)
        writer.writeheader()
        for row in rows:
            writer.writerow({field: fmt(row.get(field, "")) for field in FIELDS})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--raw-dir", type=Path, default=RAW_DIR)
    parser.add_argument("--output", type=Path, default=AUDIT_OUT)
    args = parser.parse_args()

    rows = []
    for path in sorted(args.raw_dir.glob("phase15h-r4-*-eps-ecmp-seed*-load*-flows.csv")):
        rows.extend(analyze_file(path))
    write_csv(args.output, rows)
    failed = [row for row in rows if not row["gate_pass"]]
    print(f"wrote {args.output}")
    print(f"replay audit rows: {len(rows)}; failed rows: {len(failed)}")
    if failed:
        scenarios = sorted({(row["scenario"], row["load"]) for row in failed})
        for scenario, load in scenarios:
            print(f"gate failures present for {scenario} load {load}")


if __name__ == "__main__":
    main()
