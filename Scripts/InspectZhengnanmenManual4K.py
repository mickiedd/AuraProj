"""Read-only snapshot of the exact V2 actor before manual 4K authoring."""
import json
import hashlib
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Saved/RawModelImport/ZhengnanmenManual4K"
BP = "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity/BP_GreatSouthGate_Zhengnanmen_V2_ActorAsset"
assert not (OUT / 'apply.json').exists(), 'Implementation already applied; preserve immutable rollback baseline'
bp = unreal.EditorAssetLibrary.load_asset(BP)
assert bp, BP
def mesh_file(mesh):
    path = mesh.get_path_name().split('.')[0]
    if path.startswith('/Game/'):
        return ROOT / "Content" / (path[6:] + ".uasset")
    assert path.startswith('/Engine/'), path
    return Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.engine_content_dir())) / (path[8:] + '.uasset')
ss = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
records, materials, seen = [], {}, set()
for h in ss.k2_gather_subobject_data_for_blueprint(bp):
    d = ss.k2_find_subobject_data_from_handle(h)
    if not lib.is_component(d):
        continue
    c = lib.get_object(d)
    if not isinstance(c, unreal.StaticMeshComponent) or not c.static_mesh or c.get_path_name() in seen:
        continue
    seen.add(c.get_path_name())
    mats = [c.get_material(i) for i in range(c.get_num_materials())]
    transform = {key: [float(getattr(c.get_editor_property(key), axis)) for axis in axes]
                 for key, axes in (("relative_location", "xyz"), ("relative_rotation", ("pitch", "yaw", "roll")), ("relative_scale3d", "xyz"))}
    records.append({"name": c.get_name(), "mesh": c.static_mesh.get_path_name(),
                    "instances": c.get_instance_count() if isinstance(c, unreal.InstancedStaticMeshComponent) else 1,
                    "materials": [m.get_path_name() if m else "" for m in mats],
                    "transform": transform,
                    "collision": str(c.get_collision_enabled()),
                    "mesh_sha256": hashlib.sha256(mesh_file(c.static_mesh).read_bytes()).hexdigest()})
    for m in mats:
        if m and m.get_path_name() not in materials:
            materials[m.get_path_name()] = {"name": m.get_name(), "class": m.get_class().get_name(), "textures": [
                {"path": t.get_path_name(), "width": t.blueprint_get_size_x(), "height": t.blueprint_get_size_y(),
                 "srgb": t.get_editor_property("srgb"), "compression": str(t.get_editor_property("compression_settings"))}
                for t in unreal.MaterialEditingLibrary.get_used_textures(m)]}
OUT.mkdir(parents=True, exist_ok=True)
result = {"blueprint": BP, "project": unreal.Paths.project_dir(), "world": unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name(),
          "dirty_maps": [str(p) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()],
          "components": records, "materials": materials}
(OUT / "before.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
print("MANUAL4K_BEFORE", json.dumps({"components": len(records), "instances": sum(r["instances"] for r in records), "materials": materials, "dirty_maps": result["dirty_maps"], "world": result["world"]}))
