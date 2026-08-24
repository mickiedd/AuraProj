"""Refresh embedded package bytes after the Enemy Behavior Tree migration."""

import base64
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PAIRS = (
    (
        ROOT / "Content/Blueprints/AI/BehaviorTree/BT_EnemyBehaviorTree.snapshot.json",
        ROOT / "Content/Blueprints/AI/BehaviorTree/BT_EnemyBehaviorTree.uasset",
    ),
    (
        ROOT / "Content/Blueprints/AI/BehaviorTree/BT_EnemyBehaviorTree_Elementalist.snapshot.json",
        ROOT / "Content/Blueprints/AI/BehaviorTree/BT_EnemyBehaviorTree_Elementalist.uasset",
    ),
    (
        ROOT / "Content/Blueprints/AI/Services/BTS_FindNearestHostile.snapshot.json",
        ROOT / "Content/Blueprints/AI/Services/BTS_FindNearestHostile.uasset",
    ),
)


for snapshot_path, package_path in PAIRS:
    document = json.loads(snapshot_path.read_text(encoding="utf-8"))
    package_bytes = package_path.read_bytes()
    relative_package = package_path.relative_to(ROOT).as_posix()
    entry = {
        "projectRelativePath": relative_package,
        "fileName": package_path.name,
        "byteCount": len(package_bytes),
        "contentBase64": base64.b64encode(package_bytes).decode("ascii"),
    }
    entries = document.setdefault("packageFiles", [])
    entries[:] = [item for item in entries if item.get("fileName") != package_path.name]
    entries.append(entry)
    snapshot_path.write_text(json.dumps(document, indent="\t", ensure_ascii=False) + "\n", encoding="utf-8")
    print("refreshed {}".format(snapshot_path.relative_to(ROOT)))
