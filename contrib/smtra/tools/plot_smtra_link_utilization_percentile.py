#!/usr/bin/env python3
"""Plot SMTRA p90/p95 link utilization curves from utilization-summary.csv."""

from __future__ import annotations

import argparse
import os
from itertools import cycle
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", "/tmp/matplotlib-cache")

import matplotlib.pyplot as plt
import pandas as pd
from matplotlib.ticker import AutoMinorLocator, MultipleLocator


STRATEGY_LABELS = {
    "e-only": "E_only",
    "traffic-fair": "TrafficFair",
    "strong-topk-background-shortest": "StrongTopK",
}

STYLE_CYCLE = [
    {"color": "#4C72B0", "marker": "s", "linestyle": "-"},
    {"color": "#C44E52", "marker": "^", "linestyle": "-"},
    {"color": "#55A868", "marker": "o", "linestyle": "-"},
    {"color": "#8172B2", "marker": "D", "linestyle": "-"},
    {"color": "#CCB974", "marker": "v", "linestyle": "--"},
    {"color": "#64B5CD", "marker": "P", "linestyle": "--"},
]

PERCENTILE_COLUMNS = {
    "p90": ("p90LinkUtilization", "P90 link utilization"),
    "p95": ("p95LinkUtilization", "P95 link utilization"),
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate a paper-style p90/p95 link utilization chart from SMTRA CSV data."
    )
    parser.add_argument(
        "--csv",
        type=Path,
        default=Path("utilization-summary.csv"),
        help="Input CSV path. Default: utilization-summary.csv",
    )
    parser.add_argument(
        "--percentile",
        choices=sorted(PERCENTILE_COLUMNS),
        default="p90",
        help="Utilization percentile to plot. Default: p90",
    )
    parser.add_argument(
        "--metric",
        default=None,
        help="Metric column override. Default is selected from --percentile.",
    )
    parser.add_argument(
        "--ylabel",
        default=None,
        help="Y-axis label override. Default is selected from --percentile.",
    )
    parser.add_argument(
        "--out-prefix",
        default=None,
        help="Output filename prefix. Default: <percentile>_link_utilization_vs_offered_load",
    )
    parser.add_argument(
        "--convert-percent",
        action="store_true",
        help="Convert utilization ratio to percent by multiplying by 100.",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=None,
        help="Output directory. Default: results/tables/<out-prefix>",
    )
    return parser.parse_args()


def load_data(csv_path: Path, metric: str, convert_percent: bool) -> pd.DataFrame:
    if not csv_path.exists():
        raise FileNotFoundError(f"CSV file not found: {csv_path}")

    df = pd.read_csv(csv_path)
    required = {"offeredLoad", "strategy", metric}
    missing = sorted(required - set(df.columns))
    if missing:
        raise ValueError(f"CSV is missing required columns: {', '.join(missing)}")

    df = df.copy()
    df["offeredLoad"] = pd.to_numeric(df["offeredLoad"], errors="coerce")
    df[metric] = pd.to_numeric(df[metric], errors="coerce")
    df = df.dropna(subset=["offeredLoad", "strategy", metric])
    if convert_percent:
        df[metric] = df[metric] * 100.0
    return df.sort_values(["strategy", "offeredLoad"])


def resolve_output_path(out_prefix: str, out_dir: Path | None) -> Path:
    prefix_path = Path(out_prefix)
    if out_dir is None:
        if prefix_path.parent == Path("."):
            out_dir = Path("results") / "tables" / prefix_path.name
            filename_prefix = prefix_path.name
        else:
            out_dir = prefix_path.parent
            filename_prefix = prefix_path.name
    else:
        filename_prefix = prefix_path.name

    out_dir.mkdir(parents=True, exist_ok=True)
    return out_dir / f"{filename_prefix}.png"


def configure_axes(ax: plt.Axes, ylabel: str) -> None:
    ax.set_facecolor("white")
    ax.set_xlim(0.1, 0.9)
    ax.set_xticks([round(0.1 * i, 1) for i in range(1, 10)])
    ax.xaxis.set_minor_locator(MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(AutoMinorLocator(2))

    ax.set_xlabel("offered load", fontsize=11, fontweight="bold")
    ax.set_ylabel(ylabel, fontsize=11, fontweight="bold")

    for spine in ax.spines.values():
        spine.set_visible(True)
        spine.set_linewidth(1.0)
        spine.set_color("black")

    ax.tick_params(axis="both", which="major", direction="in", length=5, width=1.0, labelsize=10)
    ax.tick_params(axis="both", which="minor", direction="in", length=3, width=0.8)
    ax.grid(True, which="major", color="#808080", linestyle=(0, (4, 3, 1, 3)), linewidth=0.9)
    ax.grid(True, which="minor", color="#B0B0B0", linestyle=(0, (2, 3)), linewidth=0.55, alpha=0.75)


def set_y_limit_with_headroom(ax: plt.Axes, values: pd.Series) -> None:
    values = values.dropna()
    if values.empty:
        return
    ymin = min(0.0, float(values.min()))
    ymax = float(values.max())
    if ymax <= ymin:
        ymax = ymin + 1.0
    ax.set_ylim(ymin, ymax + (ymax - ymin) * 0.08)


def plot_metric(df: pd.DataFrame, metric: str, ylabel: str, out_path: Path) -> None:
    plt.rcParams.update(
        {
            "figure.facecolor": "white",
            "axes.facecolor": "white",
            "font.family": "DejaVu Sans",
            "savefig.bbox": "tight",
            "savefig.pad_inches": 0.04,
        }
    )

    fig, ax = plt.subplots(figsize=(4.2, 3.4))
    style_iter = cycle(STYLE_CYCLE)
    for strategy, group in df.groupby("strategy", sort=True):
        group = group.sort_values("offeredLoad")
        style = next(style_iter)
        ax.plot(
            group["offeredLoad"],
            group[metric],
            label=STRATEGY_LABELS.get(strategy, strategy),
            color=style["color"],
            marker=style["marker"],
            linestyle=style["linestyle"],
            linewidth=2.0,
            markersize=5.0,
            markerfacecolor="white",
            markeredgewidth=1.4,
        )

    configure_axes(ax, ylabel)
    set_y_limit_with_headroom(ax, df[metric])
    legend = ax.legend(
        loc="upper left",
        bbox_to_anchor=(0.03, 0.97),
        ncol=1,
        frameon=False,
        fontsize=9,
        handlelength=2.0,
        columnspacing=1.2,
        handletextpad=0.4,
    )
    for handle in legend.legend_handles:
        handle.set_linewidth(2.0)

    fig.savefig(out_path, dpi=300)
    plt.close(fig)


def main() -> None:
    args = parse_args()
    default_metric, default_ylabel = PERCENTILE_COLUMNS[args.percentile]
    metric = args.metric or default_metric
    ylabel = args.ylabel or default_ylabel
    out_prefix = args.out_prefix or f"{args.percentile}_link_utilization_vs_offered_load"

    df = load_data(args.csv, metric, args.convert_percent)
    if df.empty:
        raise ValueError("No plottable rows after dropping NaN values.")

    out_path = resolve_output_path(out_prefix, args.out_dir)
    plot_metric(df, metric, ylabel, out_path)
    print(f"saved {out_path}")


if __name__ == "__main__":
    main()
