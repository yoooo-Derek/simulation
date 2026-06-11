#!/usr/bin/env python3
"""Phase 15H-R3 matrix-only TL-vs-Volume mechanism validation."""

import csv
import math
from pathlib import Path

PROCESSED_DIR = Path("results/processed")
MECHANISM_OUT = PROCESSED_DIR / "phase15h-r3-matrix-mechanism.csv"
SWEEP_OUT = PROCESSED_DIR / "phase15h-r3-matrix-parameter-sweep.csv"

ETAS = [0.5, 1.0, 1.5, 2.0]
ALPHAS = [0.25, 0.5, 0.75]
THETA_FS = [0.0, 25.0, 80.0]
PORTS = [1, 2]
TOP_K = 6

FIELDS = [
    "scenario",
    "num_tors",
    "optical_ports_per_tor",
    "eta",
    "alpha",
    "theta_f",
    "raw_a_top_edges",
    "p_ij_top_edges",
    "b_ij_top_edges",
    "tl_g_top_edges",
    "volume_selected_edges",
    "tl_selected_edges",
    "selected_edges_jaccard",
    "volume_selected_raw_demand_coverage",
    "tl_selected_raw_demand_coverage",
    "volume_selected_future_demand_coverage",
    "tl_selected_future_demand_coverage",
    "oracle_selected_future_demand_coverage",
    "volume_selected_tl_g_sum",
    "tl_selected_tl_g_sum",
    "volume_internal_community_edge_ratio",
    "tl_internal_community_edge_ratio",
    "community_labels",
    "corr_w_prev_d_future",
    "historical_topk_future_topk_overlap",
    "historical_selected_future_demand_coverage",
    "demand_drift_ratio",
    "selected_but_unused_lightpaths_volume",
    "selected_but_unused_lightpaths_tl",
    "oracle_possible_ocs_bytes_missed_by_volume",
    "oracle_possible_ocs_bytes_missed_by_tl",
    "mechanism_diverged",
    "non_divergence_reason",
]


def zero_matrix(n):
    return [[0.0 for _ in range(n)] for _ in range(n)]


def add_edge(matrix, i, j, value):
    matrix[i][j] += value
    matrix[j][i] += value


def clone(matrix):
    return [row[:] for row in matrix]


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
    moved_count = 0
    pass_count = 0
    for pass_index in range(max_passes):
        moved = False
        for node in range(len(matrix)):
            old = labels[node]
            candidates = [old]
            for neighbor in range(len(matrix)):
                if neighbor == node or pair_gain(matrix, node, neighbor) == 0.0:
                    continue
                neighbor_label = labels[neighbor]
                if neighbor_label not in candidates:
                    candidates.append(neighbor_label)
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
                moved_count += 1
        pass_count = pass_index + 1
        if not moved:
            break
    return normalize_labels(labels), pass_count, moved_count


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


def detect_communities(matrix, max_passes=4, max_levels=4):
    labels = list(range(len(matrix)))
    current = clone(matrix)
    for _ in range(max_levels):
        local_labels, _, _ = run_local_moving(current, max_passes=max_passes)
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


def select_volume(matrix, ports):
    return select_edges(top_edges(matrix, k=len(matrix) * len(matrix)), len(matrix), ports)


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


def select_tl(gain, labels, alpha, ports):
    return select_edges(tl_g_edges(gain, labels, alpha), len(gain), ports)


def coverage(matrix, selected):
    total = total_traffic(matrix)
    if total <= 0.0:
        return 0.0
    return sum(matrix[i][j] for i, j, _ in selected) / total


def score_sum(matrix, selected):
    return sum(matrix[i][j] for i, j, _ in selected)


def jaccard(left, right):
    left_set = edge_set(left)
    right_set = edge_set(right)
    if not left_set and not right_set:
        return 1.0
    union = left_set | right_set
    return len(left_set & right_set) / len(union) if union else 0.0


def internal_ratio(selected, labels):
    if not selected:
        return 0.0
    return sum(1 for i, j, _ in selected if labels[i] == labels[j]) / len(selected)


def fmt_edges(edges):
    return ";".join(f"{i}-{j}:{value:.3f}" for i, j, value in edges)


def fmt_labels(labels):
    return ";".join(f"{idx}:{label}" for idx, label in enumerate(labels))


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


def drift_ratio(prev, future):
    numerator = 0.0
    denominator = 0.0
    for i in range(len(prev)):
        for j in range(i + 1, len(prev)):
            numerator += abs(future[i][j] - prev[i][j])
            denominator += max(prev[i][j], 1.0)
    return numerator / denominator if denominator > 0.0 else 0.0


def selected_but_unused(selected, future):
    return sum(1 for i, j, _ in selected if future[i][j] <= 0.0)


def missed_oracle_bytes(selected, oracle, future):
    selected_pairs = edge_set(selected)
    return sum(future[i][j] for i, j, _ in oracle if (i, j) not in selected_pairs)


def diagnose_non_divergence(a, gain, labels, volume_selected, tl_selected, alpha):
    if edge_set(volume_selected) != edge_set(tl_selected):
        return ""
    raw_order = [edge[:2] for edge in top_edges(a, k=TOP_K, positive_only=True)]
    tl_order = [edge[:2] for edge in tl_g_edges(gain, labels, alpha)[:TOP_K]]
    if raw_order == tl_order:
        return "raw volume and TL-G rankings are aligned"
    if volume_selected and tl_selected and volume_selected[0][:2] == tl_selected[0][:2]:
        return "port constraint forces the same dominant edge first"
    if len(set(labels)) <= 1:
        return "community factor is ineffective because all nodes share one community"
    positives = [gain[i][j] for i in range(len(gain)) for j in range(i + 1, len(gain)) if gain[i][j] > 0]
    if not positives:
        return "thetaF/null-model leaves no positive TL-G candidates"
    return "different rankings do not survive greedy port constraints"


def make_cases():
    cases = []
    n = 8

    uniform = zero_matrix(n)
    for i in range(n):
        for j in range(i + 1, n):
            add_edge(uniform, i, j, 50.0)
    cases.append(("uniform-matrix", uniform, clone(uniform)))

    community = zero_matrix(n)
    for group in [range(0, 4), range(4, 8)]:
        group = list(group)
        for idx, i in enumerate(group):
            for j in group[idx + 1 :]:
                add_edge(community, i, j, 120.0)
    for i in range(4):
        for j in range(4, 8):
            add_edge(community, i, j, 15.0)
    future = clone(community)
    add_edge(future, 0, 1, 20.0)
    add_edge(future, 4, 5, 20.0)
    cases.append(("community-dense-block", community, future))

    aggregator = zero_matrix(n)
    for worker in [1, 2, 3, 4, 5, 6]:
        add_edge(aggregator, 0, worker, 95.0)
    for edge in [(1, 2), (2, 3), (4, 5), (5, 6)]:
        add_edge(aggregator, edge[0], edge[1], 88.0)
    for edge in [(1, 3), (4, 6)]:
        add_edge(aggregator, edge[0], edge[1], 72.0)
    add_edge(aggregator, 6, 7, 40.0)
    future = clone(aggregator)
    for worker in [1, 2, 3, 4, 5, 6]:
        future[0][worker] *= 0.35
        future[worker][0] *= 0.35
    for edge in [(1, 2), (4, 5), (5, 6)]:
        add_edge(future, edge[0], edge[1], 68.0)
    add_edge(future, 2, 3, 18.0)
    add_edge(future, 0, 3, 35.0)
    add_edge(future, 6, 7, 78.0)
    cases.append(("high-degree-aggregator-bias", aggregator, future))

    cross = zero_matrix(n)
    for edge in [(0, 1), (1, 2), (4, 5), (5, 6), (2, 3), (6, 7)]:
        add_edge(cross, edge[0], edge[1], 105.0)
    add_edge(cross, 3, 4, 132.0)
    add_edge(cross, 0, 4, 58.0)
    add_edge(cross, 1, 5, 52.0)
    add_edge(cross, 2, 6, 50.0)
    future = clone(cross)
    future[3][4] *= 0.45
    future[4][3] *= 0.45
    future[5][6] *= 0.55
    future[6][5] *= 0.55
    for edge in [(0, 1), (1, 2), (6, 7)]:
        add_edge(future, edge[0], edge[1], 40.0)
    add_edge(future, 6, 7, 35.0)
    cases.append(("cross-community-distractor", cross, future))

    mixed = zero_matrix(n)
    for worker in [2, 3, 4, 5, 6, 7]:
        add_edge(mixed, 0, worker, 75.0)
        add_edge(mixed, 1, worker, 65.0)
    for edge in [(2, 3), (3, 4), (4, 5), (5, 6), (6, 7), (2, 4), (5, 7)]:
        add_edge(mixed, edge[0], edge[1], 92.0)
    add_edge(mixed, 0, 1, 80.0)
    for edge in [(2, 6), (3, 7)]:
        add_edge(mixed, edge[0], edge[1], 38.0)
    future = clone(mixed)
    for worker in [2, 3, 4, 5, 6, 7]:
        future[0][worker] *= 0.65
        future[worker][0] *= 0.65
        future[1][worker] *= 0.7
        future[worker][1] *= 0.7
    for edge in [(2, 3), (3, 4), (5, 6), (6, 7)]:
        add_edge(future, edge[0], edge[1], 20.0)
    cases.append(("mixed-training-phase", mixed, future))

    return cases


def analyze_case(scenario, w_prev, d_future, eta, alpha, theta_f, ports):
    a = threshold(w_prev, theta_f)
    p = expected_matrix(a)
    b = modularity_gain(a, eta)
    labels = detect_communities(b)
    volume_selected = select_volume(a, ports)
    tl_selected = select_tl(b, labels, alpha, ports)
    oracle_selected = select_volume(d_future, ports)
    diverged = edge_set(volume_selected) != edge_set(tl_selected)
    reason = diagnose_non_divergence(a, b, labels, volume_selected, tl_selected, alpha)
    return {
        "scenario": scenario,
        "num_tors": len(w_prev),
        "optical_ports_per_tor": ports,
        "eta": eta,
        "alpha": alpha,
        "theta_f": theta_f,
        "raw_a_top_edges": fmt_edges(top_edges(a, positive_only=True)),
        "p_ij_top_edges": fmt_edges(top_edges(p, positive_only=True)),
        "b_ij_top_edges": fmt_edges(top_edges(b, positive_only=True)),
        "tl_g_top_edges": fmt_edges(tl_g_edges(b, labels, alpha)[:TOP_K]),
        "volume_selected_edges": fmt_edges(volume_selected),
        "tl_selected_edges": fmt_edges(tl_selected),
        "selected_edges_jaccard": jaccard(volume_selected, tl_selected),
        "volume_selected_raw_demand_coverage": coverage(a, volume_selected),
        "tl_selected_raw_demand_coverage": coverage(a, tl_selected),
        "volume_selected_future_demand_coverage": coverage(d_future, volume_selected),
        "tl_selected_future_demand_coverage": coverage(d_future, tl_selected),
        "oracle_selected_future_demand_coverage": coverage(d_future, oracle_selected),
        "volume_selected_tl_g_sum": score_sum(dict_matrix(b, len(w_prev)), volume_selected),
        "tl_selected_tl_g_sum": score_sum(dict_matrix(b, len(w_prev)), tl_selected),
        "volume_internal_community_edge_ratio": internal_ratio(volume_selected, labels),
        "tl_internal_community_edge_ratio": internal_ratio(tl_selected, labels),
        "community_labels": fmt_labels(labels),
        "corr_w_prev_d_future": pearson(w_prev, d_future),
        "historical_topk_future_topk_overlap": topk_overlap(w_prev, d_future),
        "historical_selected_future_demand_coverage": coverage(d_future, volume_selected),
        "demand_drift_ratio": drift_ratio(w_prev, d_future),
        "selected_but_unused_lightpaths_volume": selected_but_unused(volume_selected, d_future),
        "selected_but_unused_lightpaths_tl": selected_but_unused(tl_selected, d_future),
        "oracle_possible_ocs_bytes_missed_by_volume": missed_oracle_bytes(
            volume_selected, oracle_selected, d_future
        ),
        "oracle_possible_ocs_bytes_missed_by_tl": missed_oracle_bytes(
            tl_selected, oracle_selected, d_future
        ),
        "mechanism_diverged": diverged,
        "non_divergence_reason": reason,
    }


def dict_matrix(matrix, _n):
    return matrix


def fmt_value(value):
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, float):
        return f"{value:.12g}"
    return str(value)


def write_csv(path, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS)
        writer.writeheader()
        for row in rows:
            writer.writerow({field: fmt_value(row.get(field, "")) for field in FIELDS})


def main():
    rows = []
    for scenario, w_prev, d_future in make_cases():
        for eta in ETAS:
            for alpha in ALPHAS:
                for theta_f in THETA_FS:
                    for ports in PORTS:
                        rows.append(analyze_case(scenario, w_prev, d_future, eta, alpha, theta_f, ports))

    default_rows = [
        row
        for row in rows
        if row["eta"] == 1.0
        and row["alpha"] == 0.5
        and row["theta_f"] == 0.0
        and row["optical_ports_per_tor"] in (1, 2)
    ]
    write_csv(MECHANISM_OUT, default_rows)
    write_csv(SWEEP_OUT, rows)

    diverged = [row for row in rows if row["mechanism_diverged"]]
    print(f"wrote {MECHANISM_OUT}")
    print(f"wrote {SWEEP_OUT}")
    print(f"matrix rows: {len(rows)}; divergent rows: {len(diverged)}")
    for scenario in sorted({row["scenario"] for row in rows}):
        count = sum(1 for row in rows if row["scenario"] == scenario and row["mechanism_diverged"])
        print(f"{scenario}: {count} divergent parameter points")


if __name__ == "__main__":
    main()
