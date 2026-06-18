#!/usr/bin/env python3
"""Plot TL-HOC V7 community-main FCT and receiver throughput figures as SVG."""

import argparse
import csv
from html import escape
from pathlib import Path


SCHEMES = ("electrical-only", "static-ocs", "tl-hoc")
METRIC_LABELS = {
    "avg_fct_ms": ("Average FCT (ms)", "v7-community-main-avg-fct.svg"),
    "avg_receiver_throughput_gbps": (
        "Average receiver throughput (Gbps)",
        "v7-community-main-avg-receiver-throughput.svg",
    ),
}
COLORS = {
    "electrical-only": "#4c78a8",
    "static-ocs": "#f58518",
    "tl-hoc": "#54a24b",
}


def load_rows(path):
    with path.open(newline="") as handle:
        return list(csv.DictReader(handle))


def nice_ticks(max_value, count=5):
    if max_value <= 0:
        return [0.0, 1.0]
    step = max_value / (count - 1)
    return [index * step for index in range(count)]


def marker_svg(scheme, x, y, color):
    if scheme == "electrical-only":
        return f'<circle cx="{x:.2f}" cy="{y:.2f}" r="4" fill="{color}"/>'
    if scheme == "static-ocs":
        return (
            f'<rect x="{x - 4:.2f}" y="{y - 4:.2f}" width="8" height="8" '
            f'fill="{color}"/>'
        )
    return (
        f'<path d="M {x:.2f} {y - 5:.2f} L {x - 5:.2f} {y + 4:.2f} '
        f'L {x + 5:.2f} {y + 4:.2f} Z" fill="{color}"/>'
    )


def plot_metric(rows, metric, ylabel, output):
    width = 760
    height = 460
    left = 78
    right = 24
    top = 28
    bottom = 72
    plot_w = width - left - right
    plot_h = height - top - bottom

    series = {}
    all_y = []
    for scheme in SCHEMES:
        points = [row for row in rows if row["scheme"] == scheme and row["metric"] == metric]
        points.sort(key=lambda row: float(row["rho"]))
        parsed = []
        for row in points:
            mean = float(row["metric_mean"])
            low = float(row["ci95_low"])
            high = float(row["ci95_high"])
            parsed.append((float(row["rho"]), mean, low, high))
            all_y.extend([low, high, mean])
        series[scheme] = parsed

    y_max = max(all_y) if all_y else 1.0
    y_max = y_max * 1.08 if y_max > 0 else 1.0
    x_min = 0.3
    x_max = 0.9

    def sx(value):
        return left + (value - x_min) / (x_max - x_min) * plot_w

    def sy(value):
        return top + (y_max - value) / y_max * plot_h

    elements = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="white"/>',
        f'<line x1="{left}" y1="{top}" x2="{left}" y2="{top + plot_h}" '
        'stroke="#333" stroke-width="1"/>',
        f'<line x1="{left}" y1="{top + plot_h}" x2="{left + plot_w}" y2="{top + plot_h}" '
        'stroke="#333" stroke-width="1"/>',
    ]

    for tick in nice_ticks(y_max):
        y = sy(tick)
        elements.append(
            f'<line x1="{left}" y1="{y:.2f}" x2="{left + plot_w}" y2="{y:.2f}" '
            'stroke="#ddd" stroke-width="1"/>'
        )
        elements.append(
            f'<text x="{left - 10}" y="{y + 4:.2f}" text-anchor="end" '
            'font-family="Arial, sans-serif" font-size="12" fill="#333">'
            f'{tick:.3g}</text>'
        )

    for tick in (0.3, 0.5, 0.7, 0.9):
        x = sx(tick)
        elements.append(
            f'<line x1="{x:.2f}" y1="{top + plot_h}" x2="{x:.2f}" y2="{top + plot_h + 5}" '
            'stroke="#333" stroke-width="1"/>'
        )
        elements.append(
            f'<text x="{x:.2f}" y="{top + plot_h + 24}" text-anchor="middle" '
            'font-family="Arial, sans-serif" font-size="13" fill="#333">'
            f'{tick:.1f}</text>'
        )

    for scheme, points in series.items():
        if not points:
            continue
        color = COLORS[scheme]
        coords = " ".join(f'{sx(x):.2f},{sy(mean):.2f}' for x, mean, _, _ in points)
        elements.append(
            f'<polyline points="{coords}" fill="none" stroke="{color}" '
            'stroke-width="2.2" stroke-linejoin="round"/>'
        )
        for x_value, mean, low, high in points:
            x = sx(x_value)
            y = sy(mean)
            y_low = sy(low)
            y_high = sy(high)
            elements.append(
                f'<line x1="{x:.2f}" y1="{y_high:.2f}" x2="{x:.2f}" y2="{y_low:.2f}" '
                f'stroke="{color}" stroke-width="1.5"/>'
            )
            elements.append(
                f'<line x1="{x - 5:.2f}" y1="{y_high:.2f}" x2="{x + 5:.2f}" y2="{y_high:.2f}" '
                f'stroke="{color}" stroke-width="1.5"/>'
            )
            elements.append(
                f'<line x1="{x - 5:.2f}" y1="{y_low:.2f}" x2="{x + 5:.2f}" y2="{y_low:.2f}" '
                f'stroke="{color}" stroke-width="1.5"/>'
            )
            elements.append(marker_svg(scheme, x, y, color))

    elements.append(
        f'<text x="{left + plot_w / 2:.2f}" y="{height - 22}" text-anchor="middle" '
        'font-family="Arial, sans-serif" font-size="14" fill="#222">'
        'Normalized EPS load rho</text>'
    )
    elements.append(
        f'<text x="18" y="{top + plot_h / 2:.2f}" text-anchor="middle" '
        'font-family="Arial, sans-serif" font-size="14" fill="#222" '
        f'transform="rotate(-90 18 {top + plot_h / 2:.2f})">{escape(ylabel)}</text>'
    )

    legend_x = left + plot_w - 178
    legend_y = top + 12
    for index, scheme in enumerate(SCHEMES):
        y = legend_y + index * 22
        color = COLORS[scheme]
        elements.append(f'<line x1="{legend_x}" y1="{y}" x2="{legend_x + 24}" y2="{y}" '
                        f'stroke="{color}" stroke-width="2.2"/>')
        elements.append(marker_svg(scheme, legend_x + 12, y, color))
        elements.append(
            f'<text x="{legend_x + 34}" y="{y + 4}" font-family="Arial, sans-serif" '
            f'font-size="13" fill="#222">{escape(scheme)}</text>'
        )

    elements.append("</svg>")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(elements) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input",
        default="results/tables/v7-community-main-summary.csv",
        help="V7 statistical summary CSV",
    )
    parser.add_argument(
        "--output-dir",
        default="results/figures/v7-community-main",
        help="figure output directory",
    )
    args = parser.parse_args()

    input_path = Path(args.input)
    output_dir = Path(args.output_dir)
    rows = load_rows(input_path)
    for metric, (ylabel, filename) in METRIC_LABELS.items():
        output = output_dir / filename
        plot_metric(rows, metric, ylabel, output)
        print(f"PASS: wrote {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
