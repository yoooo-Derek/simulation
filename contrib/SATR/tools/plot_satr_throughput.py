#!/usr/bin/env python3
"""Plot SATR receiver throughput curves from performance-summary.csv."""

from __future__ import annotations

import argparse
import os
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", "/tmp/matplotlib-cache")

import matplotlib.pyplot as plt
import pandas as pd
from matplotlib import font_manager
from matplotlib.ticker import AutoMinorLocator, MultipleLocator


STRATEGY_LABELS = {
    "strong-topk-background-shortest": "SATR",
    "traffic-fair": "TFA",
    "e-only": "ESP",
    "SATR": "SATR",
    "TrafficFair": "TFA",
    "on-demand": "on-demand",
    "static": "static",
    "ESP": "ESP",
}

STRATEGY_ORDER = [
    "strong-topk-background-shortest",
    "traffic-fair",
    "e-only",
    "SATR",
    "TrafficFair",
    "ESP",
    "on-demand",
    "static",
]

STYLE_BY_STRATEGY = {
    "strong-topk-background-shortest": {"color": "#55A868", "marker": "o", "linestyle": "-"},
    "traffic-fair": {"color": "#C44E52", "marker": "^", "linestyle": "-"},
    "e-only": {"color": "#4C72B0", "marker": "s", "linestyle": "-"},
    "SATR": {"color": "#55A868", "marker": "o", "linestyle": "-"},
    "TrafficFair": {"color": "#C44E52", "marker": "^", "linestyle": "-"},
    "on-demand": {"color": "#8172B2", "marker": "D", "linestyle": "-"},
    "static": {"color": "#CCB974", "marker": "v", "linestyle": "-"},
    "ESP": {"color": "#4C72B0", "marker": "s", "linestyle": "-"},
}

FALLBACK_STYLES = [
    {"color": "#8172B2", "marker": "D", "linestyle": "-"},
    {"color": "#CCB974", "marker": "v", "linestyle": "-"},
    {"color": "#64B5CD", "marker": "P", "linestyle": "-"},
]

REQUESTED_PAPER_FONT_FAMILIES = [
    "Times New Roman",
    "Times",
    "Liberation Serif",
    "Nimbus Roman",
    "DejaVu Serif",
]
AVAILABLE_FONT_NAMES = {font.name for font in font_manager.fontManager.ttflist}
PAPER_FONT_FAMILIES = [
    family for family in REQUESTED_PAPER_FONT_FAMILIES if family in AVAILABLE_FONT_NAMES
] or REQUESTED_PAPER_FONT_FAMILIES
PAPER_FONT = PAPER_FONT_FAMILIES[0]
AXIS_LINE_WIDTH = 1.35
MAJOR_TICK_WIDTH = 1.25
MINOR_TICK_WIDTH = 1.0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate a paper-style throughput line chart from SATR performance CSV data."
    )
    parser.add_argument(
        "--csv",
        type=Path,
        default=Path("performance-summary.csv"),
        help="Input CSV path. Default: performance-summary.csv",
    )
    parser.add_argument(
        "--out-prefix",
        default="throughput_vs_offered_load",
        help="Output filename prefix. Default: throughput_vs_offered_load",
    )
    parser.add_argument(
        "--metric",
        default="avgReceiverThroughputGbps",
        help="Metric column to plot. Default: avgReceiverThroughputGbps",
    )
    parser.add_argument(
        "--ylabel",
        default="Throughput (Gbps)",
        help='Y-axis label. Default: "Throughput (Gbps)"',
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=None,
        help="Output directory. Default: results/tables/<out-prefix>",
    )
    parser.add_argument(
        "--panel-label",
        default=None,
        help='Optional panel label drawn below the top axis border, e.g. "(b)".',
    )
    parser.add_argument(
        "--pdf-only",
        action="store_true",
        help="Only save the PDF output, without writing the PNG.",
    )
    return parser.parse_args()


def load_data(csv_path: Path, metric: str) -> pd.DataFrame:
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


def ordered_strategies(df: pd.DataFrame) -> list[str]:
    strategies = list(dict.fromkeys(df["strategy"].dropna()))
    ordered = [strategy for strategy in STRATEGY_ORDER if strategy in strategies]
    ordered.extend(sorted(strategy for strategy in strategies if strategy not in STRATEGY_ORDER))
    return ordered


def style_for_strategy(strategy: str, fallback_index: int) -> dict[str, str]:
    if strategy in STYLE_BY_STRATEGY:
        return STYLE_BY_STRATEGY[strategy]
    return FALLBACK_STYLES[fallback_index % len(FALLBACK_STYLES)]


def configure_paper_style() -> None:
    plt.rcParams.update(
        {
            "figure.facecolor": "white",
            "axes.facecolor": "white",
            "axes.labelweight": "bold",
            "axes.linewidth": AXIS_LINE_WIDTH,
            "font.family": "serif",
            "font.serif": PAPER_FONT_FAMILIES,
            "font.weight": "bold",
            "mathtext.fontset": "custom",
            "mathtext.rm": f"{PAPER_FONT}:bold",
            "mathtext.it": f"{PAPER_FONT}:bold:italic",
            "mathtext.bf": f"{PAPER_FONT}:bold",
            "pdf.fonttype": 42,
            "ps.fonttype": 42,
        }
    )


def apply_bold_times_text(ax: plt.Axes) -> None:
    ax.xaxis.label.set_fontfamily(PAPER_FONT_FAMILIES)
    ax.xaxis.label.set_fontweight("bold")
    ax.yaxis.label.set_fontfamily(PAPER_FONT_FAMILIES)
    ax.yaxis.label.set_fontweight("bold")
    for label in ax.get_xticklabels() + ax.get_yticklabels():
        label.set_fontfamily(PAPER_FONT_FAMILIES)
        label.set_fontweight("bold")


def configure_axes(ax: plt.Axes, ylabel: str) -> None:
    ax.set_facecolor("white")
    ax.set_xlim(0.095, 0.925)
    ax.set_xticks([round(0.1 * i, 1) for i in range(1, 10)])
    ax.xaxis.set_minor_locator(MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(AutoMinorLocator(2))

    ax.set_xlabel("offered load", fontsize=13, fontfamily=PAPER_FONT_FAMILIES, fontweight="bold")
    ax.set_ylabel(ylabel, fontsize=13, fontfamily=PAPER_FONT_FAMILIES, fontweight="bold")

    for spine in ax.spines.values():
        spine.set_visible(True)
        spine.set_linewidth(AXIS_LINE_WIDTH)
        spine.set_color("black")

    ax.tick_params(axis="both", which="major", direction="in", length=5, width=MAJOR_TICK_WIDTH, labelsize=10)
    ax.tick_params(axis="both", which="minor", direction="in", length=3, width=MINOR_TICK_WIDTH)
    ax.grid(False, which="both")
    apply_bold_times_text(ax)


def set_y_limit_with_headroom(ax: plt.Axes, values: pd.Series) -> None:
    values = values.dropna()
    if values.empty:
        return
    ymin = min(0.0, float(values.min()))
    ymax = float(values.max())
    if ymax <= ymin:
        ymax = ymin + 1.0
    ax.set_ylim(ymin, ymax + (ymax - ymin) * 0.08)


def add_panel_label(ax: plt.Axes, panel_label: str | None) -> None:
    if not panel_label:
        return
    ax.text(
        0.5,
        0.935,
        panel_label,
        transform=ax.transAxes,
        ha="center",
        va="top",
        fontsize=13,
        fontfamily=PAPER_FONT_FAMILIES,
        fontweight="bold",
    )


def plot_metric(
    df: pd.DataFrame,
    metric: str,
    ylabel: str,
    out_path: Path,
    panel_label: str | None = None,
    pdf_only: bool = False,
) -> None:
    configure_paper_style()

    fig, ax = plt.subplots(figsize=(4.2, 3.2), constrained_layout=False)
    fig.subplots_adjust(left=0.18, right=0.985, bottom=0.17, top=0.97)
    fallback_index = 0
    for strategy in ordered_strategies(df):
        group = df[df["strategy"] == strategy].sort_values("offeredLoad")
        style = style_for_strategy(strategy, fallback_index)
        if strategy not in STYLE_BY_STRATEGY:
            fallback_index += 1
        ax.plot(
            group["offeredLoad"],
            group[metric],
            label=STRATEGY_LABELS.get(strategy, strategy),
            color=style["color"],
            marker=style["marker"],
            linestyle=style["linestyle"],
            linewidth=2.2,
            markersize=5.4,
            markerfacecolor="white",
            markeredgewidth=1.6,
            clip_on=False,
        )

    configure_axes(ax, ylabel)
    set_y_limit_with_headroom(ax, df[metric])
    apply_bold_times_text(ax)
    add_panel_label(ax, panel_label)
    legend = ax.legend(
        loc="upper left",
        bbox_to_anchor=(0.03, 0.97),
        ncol=1,
        frameon=False,
        prop={"family": PAPER_FONT_FAMILIES, "size": 9, "weight": "bold"},
        handlelength=2.0,
        columnspacing=1.2,
        handletextpad=0.4,
    )
    for handle in legend.legend_handles:
        handle.set_linewidth(2.4)
        handle.set_markersize(5.8)
        handle.set_markeredgewidth(1.8)

    pdf_path = out_path.with_suffix(".pdf")
    if not pdf_only:
        fig.savefig(out_path, dpi=300)
    fig.savefig(pdf_path)
    plt.close(fig)


def main() -> None:
    args = parse_args()
    df = load_data(args.csv, args.metric)
    if df.empty:
        raise ValueError("No plottable rows after dropping NaN values.")

    out_path = resolve_output_path(args.out_prefix, args.out_dir)
    plot_metric(df, args.metric, args.ylabel, out_path, args.panel_label, args.pdf_only)
    if not args.pdf_only:
        print(f"saved {out_path}")
    print(f"saved {out_path.with_suffix('.pdf')}")


if __name__ == "__main__":
    main()
