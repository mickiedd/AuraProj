"""Record the exact dirty worktree and hashes for the uncommitted M05 handoff."""
import csv
import hashlib
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Review/M05_Repository_Freeze.json"
OUT.touch(exist_ok=True)  # Include the freeze record's path in Git status.


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).rstrip("\n")


def sha(path):
    return hashlib.sha256((ROOT / path).read_bytes()).hexdigest()


manifest = "Data/M02_M05_District_Artifacts.csv"
handoff = "Review/M05_Final_Handoff.md"
archive_index = ROOT / ".claude/memory/visual-change-archive.md"
archive_lines = archive_index.read_text().splitlines()
archive_tail = next((line for line in reversed(archive_lines) if line.strip()), "")
with (ROOT / manifest).open(newline="") as stream:
    district_paths = {row["path"] for row in csv.DictReader(stream)}
status = git("status", "--porcelain=v1", "--untracked-files=all").splitlines()
status_paths = [line[3:] for line in status]
record = {
    "captured_at_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
    "base_commit": git("rev-parse", "HEAD"),
    "branch": git("branch", "--show-current"),
    "final_handoff_commit": None,
    "worktree_clean": len(status) == 0,
    "handoff_form": "uncommitted dirty worktree plus package checksums",
    "engine": "UE 5.5.4 CL 40574608",
    "host": "Apple M4 Mac mini; 16 GiB unified memory; Mac/Metal",
    "district_manifest_path": manifest,
    "district_manifest_sha256": sha(manifest),
    "district_manifest_entries": len(district_paths),
    "m05_handoff_path": handoff,
    "m05_handoff_sha256": sha(handoff),
    "visual_archive_index_snapshot": {
        "path": str(archive_index.relative_to(ROOT)),
        "line_count": len(archive_lines),
        "last_nonempty_line_sha256": hashlib.sha256(archive_tail.encode()).hexdigest(),
        "policy": "append-only outside the immutable cooked-level manifest",
    },
    "git_status_porcelain_count": len(status),
    "git_status_porcelain_sha256": hashlib.sha256(
        ("\n".join(status) + "\n").encode()).hexdigest(),
    "git_status_porcelain": status,
    "dirty_paths_in_district_manifest": sorted(set(status_paths) & district_paths),
    "dirty_paths_outside_district_manifest": sorted(set(status_paths) - district_paths),
    "warning": "No final commit exists. Reproduce from this workspace or capture a later scoped commit; preserve unrelated dirty files.",
}
OUT.write_text(json.dumps(record, indent=2) + "\n")
print("froze", len(status), "Git status entries and", len(district_paths),
      "district hashes; no commit claimed")
