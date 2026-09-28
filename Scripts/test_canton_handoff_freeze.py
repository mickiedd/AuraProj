"""Verify the two cooked Canton levels, evidence hashes and uncommitted freeze."""
import csv
import hashlib
import json
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
manifest_path = ROOT / "Data/M02_M05_District_Artifacts.csv"
freeze = json.loads((ROOT / "Review/M05_Repository_Freeze.json").read_text())
rows = list(csv.DictReader(manifest_path.open(newline="")))
by_path = {row["path"]: row for row in rows}
assert len(by_path) == len(rows), "duplicate manifest path"
assert ".claude/memory/visual-change-archive.md" not in by_path
for row in rows:
    file = ROOT / row["path"]
    data = file.read_bytes()
    assert len(data) == int(row["bytes"]), row["path"]
    assert hashlib.sha256(data).hexdigest() == row["sha256"], row["path"]

for folder in (
    "Content/Canton/DistrictPrototype",
    "Content/__ExternalActors__/Canton/DistrictPrototype",
    "Content/Canton/Provisional",
    "Content/__ExternalActors__/Canton/Provisional",
    "Content/Assets/Environment/GuangzhouLandmarks/Wenmingmen",
):
    packages = {str(file.relative_to(ROOT)) for file in (ROOT / folder).rglob("*")
                if file.suffix in (".uasset", ".umap")}
    assert packages and packages <= by_path.keys(), folder
assert "Content/Canton/Provisional/Maps/L_Canton_WalledCity_PROVISIONAL.umap" in by_path
assert "Content/Canton/DistrictPrototype/Maps/L_Canton_District_PROVISIONAL.umap" in by_path
for file in (ROOT / "QA/UE_Import_Screenshots").glob("*"):
    if file.is_file():
        assert str(file.relative_to(ROOT)) in by_path, file
for view in json.loads((ROOT / "QA/Canton_District/Capture_Settings.json").read_text()):
    primary = ROOT / view["path"]
    review = ROOT / "Review/M05_QA_Screenshots" / primary.name
    assert hashlib.sha256(primary.read_bytes()).digest() == hashlib.sha256(
        review.read_bytes()).digest(), view["view"]

sha = lambda path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest()
assert freeze["district_manifest_sha256"] == sha("Data/M02_M05_District_Artifacts.csv")
assert freeze["m05_handoff_sha256"] == sha("Review/M05_Final_Handoff.md")
assert freeze["district_manifest_entries"] == len(rows)
status = subprocess.check_output(
    ["git", "status", "--porcelain=v1", "--untracked-files=all"],
    cwd=ROOT, text=True).splitlines()
assert freeze["git_status_porcelain"] == status
index = (ROOT / ".claude/memory/visual-change-archive.md").read_text().splitlines()
index_snapshot = freeze["visual_archive_index_snapshot"]
assert index_snapshot["line_count"] <= len(index)
assert index_snapshot["last_nonempty_line_sha256"] == hashlib.sha256(
    next(line for line in reversed(index[:index_snapshot["line_count"]]) if line.strip()
        ).encode()).hexdigest()
print("Canton two-level freeze: pass (%d files, status %d entries)" %
      (len(rows), len(status)))
