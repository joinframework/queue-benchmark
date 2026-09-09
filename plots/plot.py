#!/usr/bin/env python3

import json
import sys
import re
from pathlib import Path

import numpy as np
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker

def load_json_files(paths: list[str]) -> list[dict]:
    rows = []
    for path in paths:
        with open(path) as f:
            data = json.load(f)
        for b in data.get("benchmarks", []):
            run_type = b.get("run_type", "iteration")
            agg_name = b.get("aggregate_name", "")
            if run_type == "aggregate" and agg_name != "mean":
                continue
            rows.append(b)
    return rows

def parse_bench_name(name: str) -> dict:
    name = re.sub(r"_mean$", "", name)
    parts = name.split("/")
    result = {
        "queue_type": parts[0] if len(parts) > 0 else "?",
        "pattern":    parts[1] if len(parts) > 1 else "?",
        "library":    parts[2] if len(parts) > 2 else "?",
        "args":       parts[3:] if len(parts) > 3 else [],
    }
    return result


def args_label(queue_type: str, pattern: str, args: list[str]) -> str:
    if pattern == "Bulk":
        if queue_type == "SPSC":
            return f"batch={args[0]}" if args else ""
        elif queue_type == "MPSC":
            if len(args) >= 2:
                return f"{args[0]}P batch={args[1]}"
            return "/".join(args)
        elif queue_type == "MPMC":
            if len(args) >= 3:
                return f"{args[0]}P/{args[1]}C batch={args[2]}"
            return "/".join(args)
    if queue_type == "SPSC":
        return f"cap={args[0]}" if args else ""
    elif queue_type == "MPSC":
        if len(args) >= 2:
            return f"{args[0]}P / cap={args[1]}"
        return "/".join(args)
    elif queue_type == "MPMC":
        if len(args) >= 3:
            return f"{args[0]}P/{args[1]}C / cap={args[2]}"
        return "/".join(args)
    return "/".join(args)

def build_dataframe(benches: list[dict]) -> pd.DataFrame:
    records = []
    for b in benches:
        parsed = parse_bench_name(b["name"])
        items_per_sec = b.get("items_per_second")
        real_time_ns  = b.get("real_time", 0)

        if items_per_sec is None:
            continue

        lat_min  = b.get("lat_min_ns")
        lat_mean = b.get("lat_mean_ns")
        lat_max  = b.get("lat_max_ns")
        lat_p50  = b.get("lat_p50_ns")
        lat_p90  = b.get("lat_p90_ns")
        lat_p99  = b.get("lat_p99_ns")

        records.append({
            "queue_type":      parsed["queue_type"],
            "pattern":         parsed["pattern"],
            "library":         parsed["library"],
            "args_label":      args_label(parsed["queue_type"], parsed["pattern"], parsed["args"]),
            "throughput_mops": items_per_sec / 1e6,
            "lat_min_ns":      lat_min,
            "lat_mean_ns":     lat_mean,
            "lat_max_ns":      lat_max,
            "lat_p50_ns":      lat_p50,
            "lat_p90_ns":      lat_p90,
            "lat_p99_ns":      lat_p99,
        })

    return pd.DataFrame(records)

_PALETTE = plt.rcParams["axes.prop_cycle"].by_key()["color"]
LIBRARY_COLORS: dict[str, str] = {}

def lib_color(name: str) -> str:
    if name not in LIBRARY_COLORS:
        LIBRARY_COLORS[name] = _PALETTE[len(LIBRARY_COLORS) % len(_PALETTE)]
    return LIBRARY_COLORS[name]

def bar_label_rotated(ax, bars, fmt, n_libs, base_fontsize):
    rotation = 90 if n_libs > 2 else 0
    fontsize = base_fontsize - 1 if n_libs > 2 else base_fontsize
    padding  = 8 if rotation else 3
    ax.bar_label(bars, fmt=fmt, padding=padding, fontsize=fontsize, rotation=rotation)


def plot_throughput(df: pd.DataFrame, queue_type: str, output_dir: Path) -> None:
    subset = df[(df["queue_type"] == queue_type) & (df["pattern"] == "Throughput")]
    if subset.empty:
        return

    pivot = subset.pivot_table(
        index="args_label", columns="library",
        values="throughput_mops", aggfunc="mean"
    )
    pivot = pivot.sort_index()

    fig, ax = plt.subplots(figsize=(max(8, len(pivot) * 1.5), 5))
    x = np.arange(len(pivot.index))
    n_libs = len(pivot.columns)
    width  = 0.7 / n_libs

    for i, lib in enumerate(pivot.columns):
        offset = (i - n_libs / 2 + 0.5) * width
        vals   = pivot[lib].fillna(0).values
        bars   = ax.bar(x + offset, vals, width, label=lib,
                        color=lib_color(lib), alpha=0.85, edgecolor="white")
        bar_label_rotated(ax, bars, "%.1f", n_libs, 9)

    ax.set_title(f"{queue_type} — Throughput", fontsize=13, fontweight="bold")
    ax.set_xlabel("Paramètres")
    ax.set_ylabel("Throughput (Mops/s)")
    ax.set_xticks(x)
    ax.set_xticklabels(pivot.index, rotation=20, ha="right")
    if n_libs > 2:
        ax.margins(y=0.15)
    ax.legend(title="Library", loc="upper left", bbox_to_anchor=(1.01, 1))
    ax.yaxis.set_major_formatter(mticker.FuncFormatter(lambda v, _: f"{v:.0f}"))
    ax.grid(axis="y", alpha=0.3, linestyle="--")
    ax.set_axisbelow(True)

    fig.tight_layout()
    out = output_dir / f"{queue_type.lower()}_throughput.png"
    fig.savefig(out, dpi=150, bbox_inches="tight")
    print(f"  Saved: {out}")
    plt.close(fig)


def _plot_latency_group(subset: pd.DataFrame, queue_type: str,
                        metrics: list[str], labels: list[str],
                        title_suffix: str, filename: str,
                        output_dir: Path) -> None:
    libs    = sorted(subset["library"].unique())
    params  = sorted(subset["args_label"].unique())
    n_libs   = len(libs)
    n_params = len(params)
    x        = np.arange(n_params)
    width    = 0.7 / n_libs

    subplot_w = max(4.0, n_params * 1.0 + 1.5)
    fig, axes = plt.subplots(1, len(metrics), figsize=(subplot_w * len(metrics), 6), sharey=False)
    if len(metrics) == 1:
        axes = [axes]
    fig.suptitle(f"{queue_type} — Latence {title_suffix} (ns)", fontsize=13, fontweight="bold")

    short_params = [re.sub(r"\s*/\s*cap=\d+", "", p) or queue_type for p in params]

    for ax, metric, label in zip(axes, metrics, labels):
        metric_vals = subset[metric].to_numpy()
        metric_vals = metric_vals[np.isfinite(metric_vals) & (metric_vals > 0)]
        use_log = metric_vals.size > 0 and (metric_vals.max() / metric_vals.min()) > 10

        for i, lib in enumerate(libs):
            vals = []
            for p in params:
                row = subset[(subset["library"] == lib) & (subset["args_label"] == p)]
                vals.append(row[metric].mean() if not row.empty else np.nan)
            offset = (i - n_libs / 2 + 0.5) * width
            bars = ax.bar(x + offset, vals, width, label=lib,
                          color=lib_color(lib), alpha=0.85, edgecolor="white")
            bar_label_rotated(ax, bars, "%.0f", n_libs, 9)

        ax.set_title(label)
        ax.set_xticks(x)
        ax.set_xticklabels(short_params, rotation=20, ha="right", fontsize=8)
        ax.set_ylabel("Latence (ns)")
        if use_log:
            ymin = 10 ** np.floor(np.log10(metric_vals.min()))
            ymax = 10 ** np.ceil(np.log10(metric_vals.max()))
            ax.set_yscale("log")
            ax.set_ylim(ymin, ymax * (6 if n_libs > 2 else 2))
        else:
            ax.margins(y=0.18)
        ax.yaxis.set_major_formatter(mticker.FuncFormatter(lambda v, _: f"{v:,.0f}"))
        ax.grid(axis="y", alpha=0.3, linestyle="--")
        ax.set_axisbelow(True)

    handles, legend_labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, legend_labels, title="Library", fontsize=8,
               loc="upper left", bbox_to_anchor=(1.0, 0.95))

    fig.tight_layout()
    out = output_dir / filename
    fig.savefig(out, dpi=150, bbox_inches="tight")
    print(f"  Saved: {out}")
    plt.close(fig)


def plot_latency(df: pd.DataFrame, queue_type: str, output_dir: Path) -> None:
    subset = df[(df["queue_type"] == queue_type) & (df["pattern"] == "Latency")].copy()
    if subset.empty:
        return
    subset = subset.dropna(subset=["lat_mean_ns"])

    qt = queue_type.lower()
    _plot_latency_group(
        subset, queue_type,
        metrics=["lat_min_ns", "lat_mean_ns", "lat_max_ns"],
        labels=["Min", "Mean", "Max"],
        title_suffix="Min/Mean/Max",
        filename=f"{qt}_latency_dist.png",
        output_dir=output_dir,
    )
    _plot_latency_group(
        subset, queue_type,
        metrics=["lat_p50_ns", "lat_p90_ns", "lat_p99_ns"],
        labels=["P50", "P90", "P99"],
        title_suffix="Percentiles",
        filename=f"{qt}_latency_pct.png",
        output_dir=output_dir,
    )


def plot_bulk(df: pd.DataFrame, queue_type: str, output_dir: Path) -> None:
    subset = df[(df["queue_type"] == queue_type) & (df["pattern"] == "Bulk")]
    if subset.empty:
        return

    pivot = subset.pivot_table(
        index="args_label", columns="library",
        values="throughput_mops", aggfunc="mean"
    )
    pivot = pivot.sort_index(key=lambda idx: [
        [int(x) for x in re.findall(r"\d+", s)] for s in idx
    ])

    fig, ax = plt.subplots(figsize=(max(8, len(pivot) * 1.2), 5))
    x = np.arange(len(pivot.index))
    n_libs = len(pivot.columns)
    width  = 0.7 / n_libs

    for i, lib in enumerate(pivot.columns):
        offset = (i - n_libs / 2 + 0.5) * width
        vals   = pivot[lib].fillna(0).values
        bars   = ax.bar(x + offset, vals, width, label=lib,
                        color=lib_color(lib), alpha=0.85, edgecolor="white")
        bar_label_rotated(ax, bars, "%.1f", n_libs, 9)

    ax.set_title(f"{queue_type} — Bulk Throughput", fontsize=13, fontweight="bold")
    ax.set_xlabel("Configuration")
    ax.set_ylabel("Throughput (Mops/s)")
    ax.set_xticks(x)
    ax.set_xticklabels(pivot.index, rotation=20, ha="right")
    if n_libs > 2:
        ax.margins(y=0.15)
    ax.legend(title="Library", loc="upper left", bbox_to_anchor=(1.01, 1))
    ax.yaxis.set_major_formatter(mticker.FuncFormatter(lambda v, _: f"{v:.0f}"))
    ax.grid(axis="y", alpha=0.3, linestyle="--")
    ax.set_axisbelow(True)

    fig.tight_layout()
    out = output_dir / f"{queue_type.lower()}_bulk.png"
    fig.savefig(out, dpi=150, bbox_inches="tight")
    print(f"  Saved: {out}")
    plt.close(fig)

def main() -> None:
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    json_files = sys.argv[1:]
    benches    = load_json_files(json_files)

    if not benches:
        print("Aucun résultat trouvé dans les fichiers JSON.")
        sys.exit(1)

    df = build_dataframe(benches)

    if df.empty:
        print("Aucune donnée de throughput/latence parseable.")
        sys.exit(1)

    output_dir = Path(".")

    for qtype in df["queue_type"].unique():
        plot_throughput(df, qtype, output_dir)
        plot_latency(df, qtype, output_dir)
        plot_bulk(df, qtype, output_dir)

    print("Done.")


if __name__ == "__main__":
    main()
