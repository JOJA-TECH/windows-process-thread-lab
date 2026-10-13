#!/usr/bin/env python3
"""
make_charts.py
Genera graficas a partir de measurements.csv para el laboratorio de procesos e
hilos en Windows.

Requisitos minimos:
- Python 3.8+
- matplotlib (pip install matplotlib)

Uso tipico:
    python scripts/make_charts.py --csv results/measurements.csv --out results/charts

La linea base secuencial se detecta automaticamente desde el propio CSV (media de
las filas con experiment == "secuencial"); se puede sobrescribir con --baseline-ms.
"""

from __future__ import annotations

import argparse
import csv
import statistics
import sys
from pathlib import Path

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except ImportError as exc:  # pragma: no cover
    print("matplotlib no esta disponible:", exc, file=sys.stderr)
    sys.exit(1)


DEFAULT_CSV = Path("results/measurements.csv")
DEFAULT_OUT = Path("results/charts")


def load_rows(path: Path):
    """Carga measurements.csv como lista de diccionarios."""
    if not path.exists():
        print(f"No existe {path}", file=sys.stderr)
        sys.exit(1)
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def parse_int(value, default=None):
    try:
        return int(float(value))
    except (TypeError, ValueError):
        return default


def parse_float(value, default=None):
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def mean(values):
    return statistics.mean(values) if values else 0.0


def stdev(values):
    return statistics.stdev(values) if len(values) > 1 else 0.0


def group_metric(rows, experiment, metric):
    """Agrupa una metrica (float) por numero de trabajadores."""
    groups = {}
    for row in rows:
        if row.get("experiment") != experiment:
            continue
        workers = parse_int(row.get("workers"))
        value = parse_float(row.get(metric))
        if workers is None or workers < 1 or value is None:
            continue
        groups.setdefault(workers, []).append(value)
    return groups


def detect_baseline(rows):
    """Media del tiempo de pared de las filas secuenciales (0.0 si no hay)."""
    samples = [
        parse_float(row.get("wall_time_ms"))
        for row in rows
        if row.get("experiment") == "secuencial"
    ]
    samples = [s for s in samples if s is not None]
    return mean(samples)


def save(fig, out_path: Path):
    fig.tight_layout()
    fig.savefig(out_path, dpi=110)
    plt.close(fig)
    print("Guardada:", out_path)


def plot_wall_time(groups, baseline, out_path):
    if not groups:
        print("Sin datos de hilos para", out_path.name)
        return
    workers = sorted(groups)
    means = [mean(groups[w]) for w in workers]
    stds = [stdev(groups[w]) for w in workers]

    fig, ax = plt.subplots(figsize=(8, 5))
    ax.plot(workers, means, marker="o", linewidth=2, label="Tiempo medio")
    ax.errorbar(workers, means, yerr=stds, fmt="none", ecolor="gray", capsize=3)
    if baseline > 0:
        ax.axhline(baseline, linestyle="--", linewidth=1, label=f"Linea base ({baseline:.0f} ms)")
    ax.set_xlabel("Hilos")
    ax.set_ylabel("Tiempo de pared (ms)")
    ax.set_title("Tiempo medio vs. numero de hilos")
    ax.set_xticks(workers)
    ax.grid(alpha=0.3)
    ax.legend()
    save(fig, out_path)


def plot_speedup(groups, baseline, out_path):
    if not groups or baseline <= 0:
        print("Sin datos/baseline para", out_path.name)
        return
    workers = sorted(groups)
    speedups = [baseline / mean(groups[w]) for w in workers]

    fig, ax = plt.subplots(figsize=(8, 5))
    ax.plot(workers, speedups, marker="s", linewidth=2, label="Speedup")
    ax.axhline(1.0, linestyle="--", linewidth=1, color="gray")
    ax.set_xlabel("Hilos")
    ax.set_ylabel("Speedup (linea base / tiempo)")
    ax.set_title("Speedup vs. numero de hilos")
    ax.set_xticks(workers)
    ax.grid(alpha=0.3)
    ax.legend()
    save(fig, out_path)


def plot_efficiency(groups, baseline, out_path):
    if not groups or baseline <= 0:
        print("Sin datos/baseline para", out_path.name)
        return
    workers = sorted(groups)
    efficiencies = [(baseline / mean(groups[w])) / w for w in workers]

    fig, ax = plt.subplots(figsize=(8, 5))
    ax.plot(workers, efficiencies, marker="^", linewidth=2, label="Eficiencia")
    ax.set_xlabel("Hilos")
    ax.set_ylabel("Eficiencia (speedup / hilos)")
    ax.set_title("Eficiencia vs. numero de hilos")
    ax.set_xticks(workers)
    ax.grid(alpha=0.3)
    ax.legend()
    save(fig, out_path)


def plot_threads_vs_processes(rows, out_path):
    thread_groups = group_metric(rows, "hilos", "wall_time_ms")
    process_groups = group_metric(rows, "procesos", "wall_time_ms")
    if not thread_groups and not process_groups:
        print("Sin datos de hilos/procesos para", out_path.name)
        return

    fig, ax = plt.subplots(figsize=(8, 5))
    if thread_groups:
        workers = sorted(thread_groups)
        ax.plot(workers, [mean(thread_groups[w]) for w in workers], marker="o", label="Hilos", linewidth=2)
    if process_groups:
        workers = sorted(process_groups)
        ax.plot(workers, [mean(process_groups[w]) for w in workers], marker="s", label="Procesos", linewidth=2)
    ax.set_xlabel("Trabajadores")
    ax.set_ylabel("Tiempo de pared (ms)")
    ax.set_title("Tiempo: hilos vs. procesos")
    ax.grid(alpha=0.3)
    ax.legend()
    save(fig, out_path)


def plot_cpu_utilization(rows, out_path):
    """Uso de CPU aproximado (CPU/wall) por numero de trabajadores."""
    thread_groups = group_metric(rows, "hilos", "cpu_utilization_percent")
    process_groups = group_metric(rows, "procesos", "cpu_utilization_percent")
    if not thread_groups and not process_groups:
        print("Sin datos de utilizacion de CPU para", out_path.name)
        return

    fig, ax = plt.subplots(figsize=(8, 5))
    if thread_groups:
        workers = sorted(thread_groups)
        ax.plot(workers, [mean(thread_groups[w]) for w in workers], marker="o", label="Hilos (util. proceso)", linewidth=2)
    if process_groups:
        workers = sorted(process_groups)
        ax.plot(workers, [mean(process_groups[w]) for w in workers], marker="s", label="Procesos (util. padre)", linewidth=2)
    ax.axhline(100.0, linestyle="--", linewidth=1, color="gray", label="1 nucleo (100%)")
    ax.set_xlabel("Trabajadores")
    ax.set_ylabel("Utilizacion de CPU (%)")
    ax.set_title("Uso de CPU aproximado vs. trabajadores")
    ax.grid(alpha=0.3)
    ax.legend()
    save(fig, out_path)


def main():
    parser = argparse.ArgumentParser(description="Genera graficas del laboratorio.")
    parser.add_argument("--csv", type=Path, default=DEFAULT_CSV, help="Ruta de measurements.csv")
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT, help="Directorio de graficas")
    parser.add_argument("--baseline-ms", type=float, default=None, help="Linea base en ms (auto si se omite)")
    args = parser.parse_args()

    args.out.mkdir(parents=True, exist_ok=True)
    rows = load_rows(args.csv)

    baseline = args.baseline_ms if args.baseline_ms is not None else detect_baseline(rows)
    print(f"Linea base usada: {baseline:.2f} ms")

    thread_groups = group_metric(rows, "hilos", "wall_time_ms")
    plot_wall_time(thread_groups, baseline, args.out / "wall_time_by_threads.png")
    plot_speedup(thread_groups, baseline, args.out / "speedup_by_threads.png")
    plot_efficiency(thread_groups, baseline, args.out / "efficiency_by_threads.png")
    plot_threads_vs_processes(rows, args.out / "threads_vs_processes.png")
    plot_cpu_utilization(rows, args.out / "cpu_utilization.png")

    print("Graficas generadas en", args.out)


if __name__ == "__main__":
    main()
