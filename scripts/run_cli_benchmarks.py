#!/usr/bin/env python3
"""Collect end-to-end MatrixBench CLI timings using only the Python standard library."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
import platform
import random
import subprocess
import time

SCHEMA_VERSION = "1"


def sizes(full: bool) -> list[int]:
    return [1, 2, 4, 8, 16, 32, 64, 128, 256, 512] if full else [1, 8, 32, 128]


def gauss_input(size: int, family: str, seed: int) -> str:
    rng = random.Random(seed + size)
    values: list[float] = []
    for row in range(size):
        for column in range(size):
            if family == "diagonal":
                value = 1.0 + (row % 7) * 0.01 if row == column else 0.0
            elif family == "upper_triangular":
                value = 0.0 if row > column else (1.0 if row == column else rng.uniform(-0.25, 0.25))
            else:
                value = 2.0 if row == column else rng.uniform(-0.25, 0.25)
            values.append(value)
    if family == "dense_pivot" and size > 1:
        values[0:size], values[size:2 * size] = values[size:2 * size], values[0:size]
    return f"{size}\n" + " ".join(format(value, ".17g") for value in values) + "\n"


def bareiss_input(size: int, family: str, seed: int) -> str:
    rng = random.Random(seed + size)
    values: list[int] = []
    for row in range(size):
        for column in range(size):
            if family == "diagonal":
                value = 1 if row == column else 0
            elif family == "upper_triangular":
                value = 0 if row > column else (1 if row == column else rng.randint(-2, 2))
            else:
                value = 1 if row == column else rng.randint(-1, 1)
            values.append(value)
    return f"{size}\n" + " ".join(str(value) for value in values) + "\n"


def run_case(cli: Path, storage: str, algorithm: str, payload: str) -> tuple[int, int]:
    started = time.perf_counter_ns()
    completed = subprocess.run(
        [str(cli), "--storage", storage, "--algorithm", algorithm],
        input=payload,
        text=True,
        capture_output=True,
        check=False,
        timeout=120,
    )
    return time.perf_counter_ns() - started, completed.returncode


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", required=True, type=Path, help="path to the matrixbench executable")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--mode", choices=("quick", "full"), default="quick")
    parser.add_argument("--seed", type=int, default=20261023)
    arguments = parser.parse_args()

    if not arguments.cli.is_file():
        parser.error(f"CLI executable does not exist: {arguments.cli}")
    arguments.output.mkdir(parents=True, exist_ok=True)
    samples = 30 if arguments.mode == "full" else 5
    rows: list[dict[str, object]] = []

    for algorithm, families, generator in (
        ("gauss", ("diagonal", "upper_triangular", "dense_pivot"), gauss_input),
        ("bareiss", ("diagonal", "upper_triangular", "dense"), bareiss_input),
    ):
        for size in sizes(arguments.mode == "full"):
            for family in families:
                payload = generator(size, family, arguments.seed)
                for storage in ("contiguous", "jagged"):
                    for _ in range(3):
                        run_case(arguments.cli, storage, algorithm, payload)
                    for sample in range(samples):
                        duration, return_code = run_case(arguments.cli, storage, algorithm, payload)
                        rows.append(
                            {
                                "schema_version": SCHEMA_VERSION,
                                "phase": "cli_end_to_end",
                                "algorithm": algorithm,
                                "storage": storage,
                                "family": family,
                                "size": size,
                                "magnitude": "1",
                                "seed": arguments.seed,
                                "sample": sample,
                                "batch": 1,
                                "time_ns": duration,
                                "status": "success" if return_code == 0 else "error",
                                "return_code": return_code,
                            }
                        )

    with (arguments.output / "cli_samples.csv").open("w", newline="", encoding="utf-8") as destination:
        writer = csv.DictWriter(destination, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    with (arguments.output / "cli_metadata.json").open("w", encoding="utf-8") as destination:
        json.dump(
            {
                "schema_version": SCHEMA_VERSION,
                "mode": arguments.mode,
                "seed": arguments.seed,
                "python": platform.python_version(),
                "platform": platform.platform(),
                "processor": platform.processor() or "unavailable",
                "cli": str(arguments.cli.resolve()),
            },
            destination,
            indent=2,
        )
        destination.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
