#!/usr/bin/env python3
"""Plot Phase 15F-R2 offered-load line-sweep pilot figures."""

from __future__ import annotations

import argparse
import csv
import os
import tempfile
from collections import defaultdict
from pathlib import Path


SCHEMES = ("eps-ecmp", "ocs-volume", "tl-ocs")

PLOTS = (
    ("avg_fct_s", "Average FCT (s)", "avg-fct"),
    ("p95_fct_s", "p95 FCT (s)", "p95-fct"),
    ("avg_received_throughput_bps", "Average received throughput (bps)", "throughput"),
    ("ocs_byte_hit_rate", "OCS byte hit rate", "ocs-byte-hit"),
    ("eps_avg_link_utilization", "EPS average link utilization", "eps-avg-util"),
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--aggregate",
        default="results/processed/phase15f-r2-line-summary-aggregate.csv",
        type=Path,
    )
    parser.add_argument("--output-dir", default="results/figures/phase15f-r2", type=Path)
    return parser.parse_args()


def load_rows(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        raise FileNotFoundError(
            f"{path} does not exist; run aggregate-phase15f-r2-line-sweep.py first"
        )
    with path.open(newline="") as handle:
        return list(csv.DictReader(handle))


def to_float(value: str) -> float:
    return float(value) if value else float("nan")


def scenario_slug(scenario: str) -> str:
    return scenario.replace("_", "-").replace(" ", "-")


def main() -> int:
    args = parse_args()

    os.environ.setdefault("MPLCONFIGDIR", tempfile.mkdtemp(prefix="tl-ocs-matplotlib-"))

    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    rows = load_rows(args.aggregate)
    args.output_dir.mkdir(parents=True, exist_ok=True)

    scenarios = sorted({row["scenario"] for row in rows})
    written: list[Path] = []

    for scenario in scenarios:
        scenario_rows = [row for row in rows if row["scenario"] == scenario]
        by_scheme: dict[str, list[dict[str, str]]] = defaultdict(list)
        for row in scenario_rows:
            by_scheme[row["scheme"]].append(row)
        for metric, ylabel, suffix in PLOTS:
            plt.figure(figsize=(7.5, 4.5))
            has_data = False
            for scheme in SCHEMES:
                scheme_rows = sorted(
                    by_scheme.get(scheme, []), key=lambda item: to_float(item["offered_load_factor"])
                )
                if not scheme_rows:
                    continue
                x = [to_float(row["offered_load_factor"]) for row in scheme_rows]
                y = [to_float(row[f"{metric}_mean"]) for row in scheme_rows]
                yerr = [to_float(row[f"{metric}_stddev"]) for row in scheme_rows]
                if any(value == value for value in y):
                    has_data = True
                if any(value == value and value > 0 for value in yerr):
                    plt.errorbar(x, y, yerr=yerr, marker="o", capsize=3, label=scheme)
                else:
                    plt.plot(x, y, marker="o", label=scheme)
            if not has_data:
                plt.close()
                continue
            plt.xlabel("offered_load_factor")
            plt.ylabel(ylabel)
            plt.title(f"{scenario}: {ylabel} vs offered load")
            plt.grid(True, axis="y", alpha=0.3)
            plt.legend()
            plt.tight_layout()
            output = args.output_dir / f"phase15f-r2-{scenario_slug(scenario)}-{suffix}-vs-load.png"
            plt.savefig(output, dpi=160)
            plt.close()
            written.append(output)

    for path in written:
        print(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
