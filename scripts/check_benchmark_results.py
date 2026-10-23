#!/usr/bin/env python3
"""Validate the portable smoke-level invariants of a benchmark result directory."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path


CORE_COLUMNS = {
    "schema_version", "phase", "algorithm", "storage", "family", "size",
    "magnitude", "seed", "sample", "batch", "time_ns", "status",
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=Path)
    arguments = parser.parse_args()
    directory = arguments.results
    with (directory / "metadata.json").open(encoding="utf-8") as source:
        metadata = json.load(source)
    with (directory / "core_samples.csv").open(newline="", encoding="utf-8") as source:
        rows = list(csv.DictReader(source))

    if not rows or set(rows[0]) != CORE_COLUMNS:
        raise SystemExit("core_samples.csv does not have the schema-version-1 columns")
    if {row["schema_version"] for row in rows} != {"1"}:
        raise SystemExit("core_samples.csv contains an unsupported schema version")
    if any(float(row["time_ns"]) < 0 for row in rows):
        raise SystemExit("core_samples.csv contains a negative duration")
    if {row["storage"] for row in rows} != {"contiguous", "jagged"}:
        raise SystemExit("the result set does not contain both storage layouts")
    if {int(row["seed"]) for row in rows} != {metadata["seed"]}:
        raise SystemExit("the deterministic fixtures do not use the metadata seed")
    print(f"OK: {len(rows)} core samples, schema v1, both layouts, seed {metadata['seed']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
