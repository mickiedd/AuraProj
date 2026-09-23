"""Stage the authored Wenmingmen high-detail GLB and its texture sources.

The lower-detail preview GLB is intentionally not staged: the placeable asset
uses the package's self-contained high-detail deliverable. Pass a different zip
path as the first argument when preparing on another workstation.
"""
from __future__ import annotations

import hashlib
import json
import sys
import zipfile
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
ARCHIVE = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(
    "/Volumes/M2/Works/Raw3dModels/Wenmingmen_UE5_Asset_Package.zip"
)
DESTINATION = PROJECT / "ContentSource/GuangzhouLandmarks/Wenmingmen"
PREFIX = "Wenmingmen_UE5/"


def main() -> None:
    assert ARCHIVE.is_file(), ARCHIVE
    DESTINATION.mkdir(parents=True, exist_ok=True)
    staged = []
    with zipfile.ZipFile(ARCHIVE) as bundle:
        for item in bundle.infolist():
            if item.is_dir() or not item.filename.startswith(PREFIX):
                continue
            relative = Path(item.filename[len(PREFIX):])
            if relative.name == "Wenmingmen_Interpretive.glb":
                continue
            if relative.is_absolute() or ".." in relative.parts:
                raise ValueError(f"Unsafe archive path: {item.filename}")
            target = DESTINATION / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            with bundle.open(item) as source, target.open("wb") as output:
                for block in iter(lambda: source.read(1024 * 1024), b""):
                    output.write(block)
            digest = hashlib.sha256(target.read_bytes()).hexdigest()
            staged.append({"path": relative.as_posix(), "bytes": item.file_size, "sha256": digest})
    assert any(row["path"] == "Wenmingmen_HighDetail.glb" for row in staged)
    manifest = {
        "archive_name": ARCHIVE.name,
        "archive_sha256": hashlib.sha256(ARCHIVE.read_bytes()).hexdigest(),
        "staged_files": staged,
    }
    (DESTINATION / "staging-manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(f"Staged {len(staged)} Wenmingmen source files in {DESTINATION}")


if __name__ == "__main__":
    main()
