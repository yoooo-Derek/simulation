#!/usr/bin/env python3
"""Generate Phase 15C review figures from processed aggregate CSVs."""

from __future__ import annotations

import argparse
import csv
import math
import os
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", "/tmp/tl-ocs-matplotlib-cache")

import matplotlib.pyplot as plt


SCHEMES = ("eps-ecmp", "ocs-volume", "tl-ocs")
SCHEME_LABELS = {
    "eps-ecmp": "EPS-ECMP",
    "ocs-volume": "OCS-Volume",
    "tl-ocs": "TL-OCS",
}

GROUP_LABELS = {
    "uniform-main": "Uniform",
    "community-main": "Community",
    "aggregation-main": "Aggregation",
    "aggregation-thetaf": "Aggregation thetaF",
    "community-k2": "Community k=2",
    "aggregation-k2": "Aggregation k=2",
    "aggregation-agg2": "Aggregation agg=2",
    "aggregation-agg2-thetaf": "Aggregation agg=2 thetaF",
}

DEFAULT_GROUPS = (
    "uniform-main",
    "community-main",
    "aggregation-main",
    "aggregation-thetaf",
    "community-k2",
    "aggregation-k2",
    "aggregation-agg2",
    "aggregation-agg2-thetaf",
)

METRIC_FIGURES = (
    ("avg_fct_s_mean", "Average FCT (s)", "phase15c-avg-fct-by-group.png"),
    ("p95_fct_s_mean", "P95 FCT (s)", "phase15c-p95-fct-by-group.png"),
    ("ocs_flow_hit_rate_mean", "OCS Flow Hit Rate", "phase15c-ocs-flow-hit-rate.png"),
    ("ocs_byte_hit_rate_mean", "OCS Byte Hit Rate", "phase15c-ocs-byte-hit-rate.png"),
    ("eps_avg_link_utilization_mean", "EPS Average Link Utilization", "phase15c-eps-avg-utilization.png"),
    ("ocs_avg_link_utilization_mean", "OCS Average Link Utilization", "phase15c-ocs-avg-utilization.png"),
    ("ocs_assigned_flows_mean", "OCS Assigned Flows", "phase15c-ocs-assigned-flows.png"),
    ("avg_active_edge_count_mean", "Average Active Edge Count", "phase15c-avg-active-edge-count.png"),
    ("non_empty_scheduling_rounds_mean", "Non-empty Scheduling Rounds", "phase15c-non-empty-rounds.png"),
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--processed-dir", default="results/processed", type=Path)
    parser.add_argument("--figure-dir", default="results/figures/phase15c", type=Path)
    return parser.parse_args()


def read_csv(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        raise FileNotFoundError(f"{path} not found; run aggregate-phase15c-results.py first")
    with path.open(newline="") as handle:
        return list(csv.DictReader(handle))


def to_float(value: str | None) -> float:
    if value is None or value == "":
        return 0.0
    parsed = float(value)
    if math.isnan(parsed):
        return 0.0
    return parsed


def row_index(rows: list[dict[str, str]]) -> dict[tuple[str, str], dict[str, str]]:
    return {(row["group"], row["scheme"]): row for row in rows}


def assert_metric(rows: list[dict[str, str]], metric: str) -> None:
    if not rows:
        raise ValueError("No aggregate rows found")
    if metric not in rows[0]:
        raise ValueError(f"Missing metric column: {metric}")


def plot_grouped_metric(
    rows_by_key: dict[tuple[str, str], dict[str, str]],
    all_rows: list[dict[str, str]],
    metric: str,
    ylabel: str,
    output: Path,
    groups: tuple[str, ...] = DEFAULT_GROUPS,
) -> None:
    assert_metric(all_rows, metric)
    x_positions = list(range(len(groups)))
    bar_width = 0.24
    offsets = (-bar_width, 0.0, bar_width)

    fig, ax = plt.subplots(figsize=(12, 5.5))
    for scheme, offset in zip(SCHEMES, offsets):
        values = [
            to_float(rows_by_key[(group, scheme)].get(metric))
            if (group, scheme) in rows_by_key
            else 0.0
            for group in groups
        ]
        ax.bar(
            [position + offset for position in x_positions],
            values,
            width=bar_width,
            label=SCHEME_LABELS[scheme],
        )

    ax.set_title(ylabel)
    ax.set_ylabel(ylabel)
    ax.set_xticks(x_positions)
    ax.set_xticklabels([GROUP_LABELS.get(group, group) for group in groups], rotation=35, ha="right")
    ax.legend()
    ax.grid(axis="y", linestyle=":", linewidth=0.8)
    fig.tight_layout()
    fig.savefig(output, dpi=180)
    plt.close(fig)


def plot_aggregation_thetaf_sensitivity(
    rows_by_key: dict[tuple[str, str], dict[str, str]],
    output: Path,
) -> None:
    groups = ("aggregation-main", "aggregation-thetaf")
    metrics = ("ocs_assigned_flows_mean", "avg_fct_s_mean")
    titles = ("OCS Assigned Flows", "Average FCT (s)")
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.8))
    bar_width = 0.36
    x_positions = list(range(len(groups)))

    for ax, metric, title in zip(axes, metrics, titles):
        for index, scheme in enumerate(("ocs-volume", "tl-ocs")):
            offset = -bar_width / 2 if index == 0 else bar_width / 2
            values = [to_float(rows_by_key[(group, scheme)].get(metric)) for group in groups]
            ax.bar([position + offset for position in x_positions], values, width=bar_width, label=SCHEME_LABELS[scheme])
        ax.set_title(title)
        ax.set_ylabel(title)
        ax.set_xticks(x_positions)
        ax.set_xticklabels(("thetaF=0", "thetaF=50000"), rotation=0)
        ax.grid(axis="y", linestyle=":", linewidth=0.8)
        ax.legend()

    fig.suptitle("Parameter-Aggregation thetaF Sensitivity")
    fig.tight_layout()
    fig.savefig(output, dpi=180)
    plt.close(fig)


def plot_k_sensitivity(rows_by_key: dict[tuple[str, str], dict[str, str]], output: Path) -> None:
    groups = ("community-main", "community-k2", "aggregation-main", "aggregation-k2")
    fig, ax = plt.subplots(figsize=(9.5, 5.0))
    bar_width = 0.36
    x_positions = list(range(len(groups)))
    for index, scheme in enumerate(("ocs-volume", "tl-ocs")):
        offset = -bar_width / 2 if index == 0 else bar_width / 2
        values = [to_float(rows_by_key[(group, scheme)].get("ocs_flow_hit_rate_mean")) for group in groups]
        ax.bar([position + offset for position in x_positions], values, width=bar_width, label=SCHEME_LABELS[scheme])

    ax.set_title("k Sensitivity: OCS Flow Hit Rate")
    ax.set_ylabel("OCS Flow Hit Rate")
    ax.set_xticks(x_positions)
    ax.set_xticklabels(("Community k=1", "Community k=2", "Aggregation k=1", "Aggregation k=2"), rotation=20, ha="right")
    ax.grid(axis="y", linestyle=":", linewidth=0.8)
    ax.legend()
    fig.tight_layout()
    fig.savefig(output, dpi=180)
    plt.close(fig)


def main() -> None:
    args = parse_args()
    summary_path = args.processed_dir / "phase15c-summary-aggregate.csv"
    comparison_path = args.processed_dir / "phase15c-scheme-comparison.csv"
    summary_rows = read_csv(summary_path)
    _ = read_csv(comparison_path)

    rows_by_key = row_index(summary_rows)
    args.figure_dir.mkdir(parents=True, exist_ok=True)

    outputs: list[Path] = []
    for metric, ylabel, filename in METRIC_FIGURES:
        output = args.figure_dir / filename
        plot_grouped_metric(rows_by_key, summary_rows, metric, ylabel, output)
        outputs.append(output)

    thetaf_output = args.figure_dir / "phase15c-aggregation-thetaf-sensitivity.png"
    plot_aggregation_thetaf_sensitivity(rows_by_key, thetaf_output)
    outputs.append(thetaf_output)

    k_output = args.figure_dir / "phase15c-k-sensitivity.png"
    plot_k_sensitivity(rows_by_key, k_output)
    outputs.append(k_output)

    print("PHASE15C_FIGURE_GENERATION_PASS")
    for output in outputs:
        print(output)


if __name__ == "__main__":
    main()
