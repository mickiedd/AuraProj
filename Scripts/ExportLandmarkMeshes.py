"""Export the meshes the nine showcase landmarks render, in batches, to OBJ.

The reference-tuning skill records vertex readback as blocked in UE 5.5 Python
(`get_vertex_position` needs a `VertexID` that cannot be constructed), and earlier
passes worked around it by parsing the source GLB. That is not necessary:
`AssetExportTask` + `Exporter.run_asset_export_task` exports the *actual project
mesh* to OBJ, which is the better target for an audit because it is what renders.

This is stage one of two. It only exports; the geometry analysis runs outside the
editor, where numpy is available, and deletes each OBJ after measuring it.

Batches keep disk use bounded: the editor has no numpy, so the OBJ has to reach
disk, but there is no need for more than one batch to exist at a time. Progress is
recorded so repeated invocations walk the whole set.
"""

import json
import os
from pathlib import Path

import unreal

AUDIT_DIR = Path("C:/Git/AuraProj/Saved/MeshAudit")
OBJ_DIR = AUDIT_DIR / "obj"
PROGRESS = AUDIT_DIR / "progress.json"

BATCH_SIZE = 25

LANDMARKS = [
    ("Zhengnanmen_HighFidelity",
     "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity/BP_GreatSouthGate_Zhengnanmen_V2_ActorAsset"),
    ("Zhengnanmen_AAA_V3",
     "/Game/Assets/Environment/GuangzhouLandmarks/V3/Zhengnanmen_AAA_V3/BP_Zhengnanmen_AAA_V3"),
    ("Xiaobeimen_AAA_V3",
     "/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_AAA_V3/BP_Xiaobeimen_AAA_V3"),
    ("Xiaobeimen_Production_V3",
     "/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_Production_V3/BP_Xiaobeimen_Production_V3"),
    ("Guidemen_V5_4K",
     "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/BP_Guidemen_V5_4K"),
    ("Wuxianmen_V5_4K_Core",
     "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core"),
    ("Wuxianmen_V5_FullPBR",
     "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_FullPBR/BP_Wuxianmen_V5_FullPBR"),
    ("GreatNorthGate",
     "/Game/Assets/Environment/GuangzhouLandmarks/GreatNorthGate/BP_GreatNorthGate"),
    ("ZhenhaiTower",
     "/Game/Assets/Environment/GuangzhouLandmarks/ZhenhaiTower/BP_ZhenhaiTower"),
]


def collect_meshes():
    """Map each unique rendered mesh path to the landmarks that use it."""
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    owners = {}
    for landmark, blueprint_path in LANDMARKS:
        blueprint = unreal.EditorAssetLibrary.load_asset(blueprint_path)
        assert isinstance(blueprint, unreal.Blueprint), blueprint_path
        actor = actor_subsystem.spawn_actor_from_class(
            blueprint.generated_class(), unreal.Vector(0, 0, 0),
            unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), transient=True)
        assert actor, blueprint_path
        try:
            for component in actor.get_components_by_class(unreal.StaticMeshComponent):
                mesh = component.static_mesh
                if mesh is None:
                    continue
                try:
                    if not bool(component.get_editor_property("visible")):
                        continue
                except Exception:
                    pass
                path = mesh.get_path_name().split(".")[0]
                owners.setdefault(path, []).append(landmark)
        finally:
            actor_subsystem.destroy_actor(actor)
    return owners


def export_one(mesh_path, filename):
    mesh = unreal.EditorAssetLibrary.load_asset(mesh_path)
    if mesh is None:
        return {"mesh": mesh_path, "error": "load failed"}
    task = unreal.AssetExportTask()
    task.set_editor_property("object", mesh)
    task.set_editor_property("filename", filename)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_identical", True)
    task.set_editor_property("prompt", False)
    try:
        unreal.Exporter.run_asset_export_task(task)
    except Exception as exc:
        return {"mesh": mesh_path, "error": "export failed: {}".format(exc)}
    if not os.path.exists(filename):
        return {"mesh": mesh_path, "error": "no file written"}
    return {"mesh": mesh_path, "obj": filename, "bytes": os.path.getsize(filename)}


def main():
    AUDIT_DIR.mkdir(parents=True, exist_ok=True)
    OBJ_DIR.mkdir(parents=True, exist_ok=True)

    owners = collect_meshes()
    all_meshes = sorted(owners)
    done = set()
    if PROGRESS.exists():
        done = set(json.loads(PROGRESS.read_text(encoding="utf-8")).get("done", []))

    pending = [path for path in all_meshes if path not in done]
    batch = pending[:BATCH_SIZE]

    results = []
    for mesh_path in batch:
        filename = str(OBJ_DIR / (mesh_path.rsplit("/", 1)[-1] + ".obj"))
        entry = export_one(mesh_path, filename)
        entry["landmarks"] = sorted(set(owners[mesh_path]))
        results.append(entry)

    done.update(entry["mesh"] for entry in results if "obj" in entry)
    PROGRESS.write_text(json.dumps({
        "total": len(all_meshes),
        "done": sorted(done),
        "owners": owners,
    }, indent=2), encoding="utf-8")

    failed = [entry for entry in results if "error" in entry]
    report = {
        "unique_meshes": len(all_meshes),
        "exported_this_batch": len(results),
        "remaining_after_batch": len(all_meshes) - len(done),
        "failed": failed,
        "batch": results,
    }
    unreal.log("MESH_EXPORT_BATCH " + json.dumps(report))
    print("MESH_EXPORT_SUMMARY", json.dumps({k: v for k, v in report.items()
                                             if k != "batch"}))
    for entry in results:
        print("MESH_EXPORT", json.dumps(entry))


if __name__ == "__main__":
    main()
