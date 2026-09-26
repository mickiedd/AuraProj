"""Validate handoff integrity and fail-closed historical status."""

import csv
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main():
    errors = []
    index_path = ROOT / "FILE_INDEX.csv"
    manifest_path = ROOT / "MANIFEST.sha256"
    if not index_path.is_file() or not manifest_path.is_file():
        raise SystemExit("Packet manifest files are missing")

    with index_path.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    indexed = {row["path"] for row in rows}
    if len(indexed) != len(rows):
        errors.append("FILE_INDEX.csv contains duplicate paths")
    for row in rows:
        path = ROOT / row["path"]
        if not path.is_file():
            errors.append("Missing: " + row["path"])
            continue
        data = path.read_bytes()
        if len(data) != int(row["bytes"]):
            errors.append("Size mismatch: " + row["path"])
        if hashlib.sha256(data).hexdigest() != row["sha256"]:
            errors.append("SHA-256 mismatch: " + row["path"])

    listed = set()
    for line in manifest_path.read_text(encoding="utf-8").splitlines():
        digest, relative = line.split("  ", 1)
        listed.add(relative)
        path = ROOT / relative
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            errors.append("MANIFEST mismatch: " + relative)
    if listed != indexed:
        errors.append("MANIFEST.sha256 and FILE_INDEX.csv cover different files")

    transform = json.loads((ROOT / "context/Data/Map_Transform_Provisional.json").read_text())
    contract = json.loads((ROOT / "context/Data/Coordinate_Contract.json").read_text())
    if transform.get("status") != "PROVISIONAL_FAILS_DAY_03_ACCEPTANCE":
        errors.append("Failed transform status was changed")
    if transform.get("acceptance", {}).get("passed") is not False:
        errors.append("Failed transform was promoted")
    if contract.get("terrain_zero_elevation_m") is not None:
        errors.append("Historical terrain zero was populated without accepted evidence")

    with (ROOT / "context/Data/Elevation_Constraints.csv").open(newline="", encoding="utf-8") as stream:
        constraints = list(csv.DictReader(stream))
    if any(row.get("status") != "candidate_stratum_not_terrain" for row in constraints):
        errors.append("Existing stratigraphic candidate was promoted")

    print(json.dumps({
        "packet_valid": not errors,
        "file_count": len(rows),
        "errors": errors,
        "horizontal_gate": "Blocked in supplied baseline",
        "vertical_gate": "Blocked in supplied baseline",
    }, indent=2))
    raise SystemExit(1 if errors else 0)


if __name__ == "__main__":
    main()
