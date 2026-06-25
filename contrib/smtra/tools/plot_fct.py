#!/usr/bin/env python3
"""Plot SMTRA metric curves from performance-summary.csv.

The default target is the FCT curve used in the paper-style figures:
offered load on the x-axis and average FCT in milliseconds on the y-axis.
"""

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
    {"color": "#4C72B0", "marker": "s", "linestyle": "-"},   # blue square
    {"color": "#C44E52", "marker": "^", "linestyle": "-"},   # red triangle
    {"color": "#55A868", "marker": "o", "linestyle": "-"},   # green circle
    {"color": "#8172B2", "marker": "D", "linestyle": "-"},   # purple diamond
    {"color": "#CCB974", "marker": "v", "linestyle": "--"},
    {"color": "#64B5CD", "marker": "P", "linestyle": "--"},
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate a paper-style line chart from SMTRA performance CSV data."
    )
    parser.add_argument(
        "--csv",
        type=Path,
        default=Path("performance-summary.csv"),
        help="Input CSV path. Default: performance-summary.csv",
    )
    parser.add_argument(
        "--out-prefix",
        default="fct_vs_offered_load",
        help="Output filename prefix. Default: fct_vs_offered_load",
    )
    parser.add_argument(
        "--metric",
        default="avgFctSeconds",
        help="Metric column to plot. Default: avgFctSeconds",
    )
    parser.add_argument(
        "--ylabel",
        default="FCT (ms)",
        help='Y-axis label. Default: "FCT (ms)"',
    )
    parser.add_argument(
        "--convert-ms",
        dest="convert_ms",
        action="store_true",
        default=True,
        help="Convert seconds to milliseconds. Enabled by default.",
    )
    parser.add_argument(
        "--no-convert-ms",
        dest="convert_ms",
        action="store_false",
        help="Disable seconds-to-milliseconds conversion.",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=None,
        help="Output directory. Default: results/tables/<out-prefix>",
    )
    return parser.parse_args()


def load_data(csv_path: Path, metric: str, convert_ms: bool) -> pd.DataFrame:
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

    # Drop only rows that cannot be plotted. This handles NA/NaN FCT values safely.
    df = df.dropna(subset=["offeredLoad", "strategy", metric])
    if convert_ms:
        df[metric] = df[metric] * 1000.0
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

    # Match the reference: visible outer box, inward ticks, grey dash-dot grids.
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
    headroom = (ymax - ymin) * 0.08
    ax.set_ylim(ymin, ymax + headroom)


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
    df = load_data(args.csv, args.metric, args.convert_ms)
    if df.empty:
        raise ValueError("No plottable rows after dropping NaN values.")

    out_path = resolve_output_path(args.out_prefix, args.out_dir)
    plot_metric(df, args.metric, args.ylabel, out_path)
    print(f"saved {out_path}")


if __name__ == "__main__":
    main()
