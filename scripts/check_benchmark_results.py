#!/usr/bin/env python3
"""Validate and normalize MatrixBench schema-v2 Google Benchmark JSON."""
from __future__ import annotations
import argparse
import json
import math
from pathlib import Path


def load_core(directory: Path):
    metadata = json.loads((directory / "metadata.json").read_text())
    if metadata.get("schema_version") != "2":
        raise ValueError("expected MatrixBench schema v2; regenerate old CSV results")
    if metadata.get("output_format") != "json":
        raise ValueError("the report requires JSON output")
    document = json.loads(Path(metadata["output_path"]).read_text())
    if document["context"].get("seed") != str(metadata["seed"]):
        raise ValueError("fixture seed does not match metadata")
    rows = []
    for result in document["benchmarks"]:
        if result.get("error_occurred"):
            raise ValueError(f"benchmark failed: {result.get('error_message')}")
        if result.get("run_type", "iteration") != "iteration":
            continue
        name = result.get("run_name", result["name"]).split("/")
        phase, algorithm, storage, family, size = name[:5]
        if storage not in ("contiguous", "jagged") or phase not in ("construct", "copy", "fill_row_major", "determinant"):
            raise ValueError("unknown benchmark case")
        factor = {"ns": 1, "us": 1000, "ms": 1000000, "s": 1000000000}[result["time_unit"]]
        for key in ("cpu_time", "real_time"):
            if not math.isfinite(result[key]) or result[key] < 0:
                raise ValueError("invalid timing")
        if result["iterations"] <= 0:
            raise ValueError("invalid iteration count")
        rows.append(dict(phase=phase, algorithm=algorithm, storage=storage, family=family,
                         size=int(size), magnitude="1", seed=str(metadata["seed"]),
                         sample=result.get("repetition_index", 0), batch=result["iterations"],
                         time_ns=result["cpu_time"] * factor, real_time_ns=result["real_time"] * factor,
                         status="success"))
    if not rows:
        raise ValueError("no individual results; disable --benchmark_report_aggregates_only")
    return metadata, rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=Path)
    args = parser.parse_args()
    try:
        metadata, rows = load_core(args.results)
    except (ValueError, KeyError, OSError) as error:
        parser.exit(2, f"invalid benchmark results: {error}\n")
    print(f"OK: {len(rows)} core samples, schema v2, seed {metadata['seed']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
