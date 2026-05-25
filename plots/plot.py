#!/usr/bin/env python3

import json
import sys
import re
from pathlib import Path

import numpy as np
import pandas as pd
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

        lat_mean = b.get("lat_mean_ns")
        lat_p50  = b.get("lat_p50_ns")
        lat_p99  = b.get("lat_p99_ns")

        records.append({
            "queue_type":      parsed["queue_type"],
            "pattern":         parsed["pattern"],
            "library":         parsed["library"],
            "args_label":      args_label(parsed["queue_type"], parsed["pattern"], parsed["args"]),
            "throughput_mops": items_per_sec / 1e6,
            "lat_mean_ns":     lat_mean,
            "lat_p50_ns":      lat_p50,
            "lat_p99_ns":      lat_p99,
        })

    return pd.DataFrame(records)

_PALETTE = plt.rcParams["axes.prop_cycle"].by_key()["color"]
LIBRARY_COLORS: dict[str, str] = {}

def lib_color(name: str) -> str:
    if name not in LIBRARY_COLORS:
        LIBRARY_COLORS[name] = _PALETTE[len(LIBRARY_COLORS) % len(_PALETTE)]
    return LIBRARY_COLORS[name]


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
        ax.bar_label(bars, fmt="%.1f", padding=3, fontsize=8)

    ax.set_title(f"{queue_type} — Throughput", fontsize=13, fontweight="bold")
    ax.set_xlabel("Paramètres")
    ax.set_ylabel("Throughput (Mops/s)")
    ax.set_xticks(x)
    ax.set_xticklabels(pivot.index, rotation=20, ha="right")
    ax.legend(title="Library")
    ax.yaxis.set_major_formatter(mticker.FuncFormatter(lambda v, _: f"{v:.0f}"))
    ax.grid(axis="y", alpha=0.3, linestyle="--")
    ax.set_axisbelow(True)

    fig.tight_layout()
    out = output_dir / f"{queue_type.lower()}_throughput.png"
    fig.savefig(out, dpi=150)
    print(f"  Saved: {out}")
    plt.close(fig)


def plot_latency(df: pd.DataFrame, queue_type: str, output_dir: Path) -> None:
    subset = df[(df["queue_type"] == queue_type) & (df["pattern"] == "Latency")].copy()
    if subset.empty:
        return
    subset = subset.dropna(subset=["lat_mean_ns"])

    libs    = sorted(subset["library"].unique())
    params  = sorted(subset["args_label"].unique())
    metrics = ["lat_mean_ns", "lat_p50_ns", "lat_p99_ns"]
    labels  = ["Mean", "P50", "P99"]

    n_libs   = len(libs)
    n_params = len(params)
    x        = np.arange(n_params)
    width    = 0.7 / n_libs

    subplot_w = max(4.0, n_params * 1.0 + 1.5)
    fig, axes = plt.subplots(1, len(metrics), figsize=(subplot_w * len(metrics), 6), sharey=False)
    fig.suptitle(f"{queue_type} — Latence (ns)", fontsize=13, fontweight="bold")

    short_params = [re.sub(r"\s*/\s*cap=\d+", "", p) or queue_type for p in params]

    for ax, metric, label in zip(axes, metrics, labels):
        for i, lib in enumerate(libs):
            vals = []
            for p in params:
                row = subset[(subset["library"] == lib) & (subset["args_label"] == p)]
                vals.append(row[metric].mean() if not row.empty else np.nan)
            offset = (i - n_libs / 2 + 0.5) * width
            bars = ax.bar(x + offset, vals, width, label=lib,
                          color=lib_color(lib), alpha=0.85, edgecolor="white")
            ax.bar_label(bars, fmt="%.0f", padding=2, fontsize=6)

        ax.set_title(label)
        ax.set_xticks(x)
        ax.set_xticklabels(short_params, rotation=20, ha="right", fontsize=8)
        ax.set_ylabel("Latence (ns)")
        ax.set_yscale("log")
        ax.yaxis.set_major_formatter(mticker.FuncFormatter(lambda v, _: f"{v:,.0f}"))
        ax.legend(title="Library", fontsize=8)
        ax.grid(axis="y", alpha=0.3, linestyle="--")
        ax.set_axisbelow(True)

    fig.tight_layout()
    out = output_dir / f"{queue_type.lower()}_latency.png"
    fig.savefig(out, dpi=150)
    print(f"  Saved: {out}")
    plt.close(fig)


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
        ax.bar_label(bars, fmt="%.1f", padding=3, fontsize=8)

    ax.set_title(f"{queue_type} — Bulk Throughput", fontsize=13, fontweight="bold")
    ax.set_xlabel("Configuration")
    ax.set_ylabel("Throughput (Mops/s)")
    ax.set_xticks(x)
    ax.set_xticklabels(pivot.index, rotation=20, ha="right")
    ax.legend(title="Library")
    ax.yaxis.set_major_formatter(mticker.FuncFormatter(lambda v, _: f"{v:.0f}"))
    ax.grid(axis="y", alpha=0.3, linestyle="--")
    ax.set_axisbelow(True)

    fig.tight_layout()
    out = output_dir / f"{queue_type.lower()}_bulk.png"
    fig.savefig(out, dpi=150)
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
