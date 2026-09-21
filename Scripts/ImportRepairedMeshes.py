"""Import the repaired landmark meshes and verify the round trip.

Stage three of the geometry repair. The repaired OBJs were written from the meshes
exported out of Unreal, so the round trip has to be proven before anything is
rebound: if the importer applies a different axis convention or scale, the repaired
mesh would land rotated or mirrored and silently break the asset.

Verification is by bounds, not by eye: the imported mesh must reproduce the
original's local bounds within a tight tolerance. An axis swap or a mirror changes
the bounds, so this catches the failure modes that matter.

Imports only. Rebinding is a separate, explicit step.
"""

import json
from pathlib import Path

import unreal

REPAIRED_DIR = Path("C:/Git/AuraProj/Saved/MeshAudit/repaired")
REPAIR_REPORT = Path("C:/Git/AuraProj/Saved/MeshAudit/repair-report.json")
PROGRESS = Path("C:/Git/AuraProj/Saved/MeshAudit/progress.json")
REPORT = Path("C:/Git/AuraProj/Saved/MeshAudit/import-report.json")

DEST = "/Game/Assets/Environment/GuangzhouLandmarks/Repair20260921"
BOUNDS_TOLERANCE_CM = 0.5


def original_mesh_path(asset_name):
    owners = json.loads(PROGRESS.read_text(encoding="utf-8"))["owners"]
    for path in owners:
        if path.rsplit("/", 1)[-1] == asset_name:
            return path
    return None


def bounds_of(mesh):
    box = mesh.get_bounds()
    origin, extent = box.origin, box.box_extent
    return ([float(origin.x) - float(extent.x), float(origin.y) - float(extent.y),
             float(origin.z) - float(extent.z)],
            [float(origin.x) + float(extent.x), float(origin.y) + float(extent.y),
             float(origin.z) + float(extent.z)])


def main():
    repair = json.loads(REPAIR_REPORT.read_text(encoding="utf-8"))
    report = {}
    pending = []
    for name, stats in sorted(repair.items()):
        if "out" not in stats:
            report[name] = {"error": stats.get("error", "not repaired")}
            continue
        destination = "{}/SM_{}_Repaired".format(DEST, name)
        if unreal.EditorAssetLibrary.does_asset_exist(destination):
            report[name] = {"status": "already_imported", "asset": destination}
            continue
        pending.append((name, stats))

    if pending:
        tasks = []
        for name, stats in pending:
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", stats["out"])
            task.set_editor_property("destination_path", DEST)
            task.set_editor_property("destination_name", "SM_{}_Repaired".format(name))
            task.set_editor_property("automated", True)
            task.set_editor_property("save", True)
            task.set_editor_property("replace_existing", False)
            tasks.append((name, task))
        try:
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(
                [task for _, task in tasks])
        except Exception as exc:
            for name, _ in tasks:
                report[name] = {"error": "import failed: {}".format(exc)}

    for name, stats in sorted(repair.items()):
        if name in report:
            continue
        destination = "{}/SM_{}_Repaired".format(DEST, name)
        repaired_mesh = unreal.EditorAssetLibrary.load_asset(destination)
        if repaired_mesh is None:
            report[name] = {"error": "asset not found after import"}
            continue
        source_path = original_mesh_path(name)
        original = unreal.EditorAssetLibrary.load_asset(source_path) if source_path else None
        entry = {
            "status": "imported",
            "asset": destination,
            "source_mesh": source_path,
            "faces_expected": stats["faces_after"],
            "repaired_bounds": [[round(v, 2) for v in bounds_of(repaired_mesh)[0]],
                                [round(v, 2) for v in bounds_of(repaired_mesh)[1]]],
        }
        if original is not None:
            original_bounds = bounds_of(original)
            entry["original_bounds"] = [[round(v, 2) for v in original_bounds[0]],
                                        [round(v, 2) for v in original_bounds[1]]]
            worst = max(abs(a - b) for a, b in zip(bounds_of(repaired_mesh)[0],
                                                   original_bounds[0])
                        ) if True else 0.0
            worst = max(worst, max(abs(a - b) for a, b in zip(bounds_of(repaired_mesh)[1],
                                                              original_bounds[1])))
            entry["bounds_delta_cm"] = round(worst, 4)
            entry["bounds_match"] = worst <= BOUNDS_TOLERANCE_CM
        report[name] = entry
        print("IMPORT_VERIFY", name, json.dumps(entry))

    REPORT.write_text(json.dumps(report, indent=2, sort_keys=True), encoding="utf-8")
    matched = sum(1 for e in report.values() if e.get("bounds_match"))
    failed = [n for n, e in report.items() if not e.get("bounds_match")]
    print("IMPORT_TOTAL", len(report), "bounds_matched", matched)
    print("IMPORT_FAILED", json.dumps(failed))
    print("IMPORT_REPORT", REPORT)


if __name__ == "__main__":
    main()
