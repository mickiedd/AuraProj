"""Restore the Guidemen materials after the flat-grey test, then remove every stray transient actor."""
import json
from pathlib import Path
import unreal

PACKAGE = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K"
BLUEPRINT = PACKAGE + "/BP_Guidemen_V5_4K"
ROOT = Path("C:/Git/AuraProj/Saved/RawModelImport/V5/GuidemenRebuild")
EAL = unreal.EditorAssetLibrary

# 1. restore the real materials (the flat-grey probe left them all grey)
bp = EAL.load_asset(BLUEPRINT)
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
seen, comps = set(), []
for h in sub.k2_gather_subobject_data_for_blueprint(bp):
    c = lib.get_object(sub.k2_find_subobject_data_from_handle(h))
    if not isinstance(c, unreal.HierarchicalInstancedStaticMeshComponent):
        continue
    if c.get_path_name() in seen: continue
    seen.add(c.get_path_name()); comps.append(c)
by_name = {c.get_name(): c for c in comps}
record = json.loads((ROOT / "flatgrey-bindings.json").read_text())
for item in record:
    c = by_name.get(item["c"])
    if c:
        c.set_material(item["i"], EAL.load_asset(item["m"]))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
EAL.save_loaded_asset(bp, only_if_is_dirty=False)
print("MATERIALS_RESTORED", len(record))

# 2. remove every stray transient actor this session left behind
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
removed = []
for a in list(actors.get_all_level_actors()):
    n = a.get_name()
    if n.startswith("BP_Guidemen") or n.startswith("SceneCapture") or n.startswith("DirectionalLight") \
       or n.startswith("SkyLight") or n.startswith("SkyAtmosphere") or n.startswith("PostProcessVolume") \
       or n.startswith("StaticMeshActor"):
        removed.append(n)
        actors.destroy_actor(a)
remaining = [a.get_name() for a in actors.get_all_level_actors()]
print("CLEANUP_REMOVED", len(removed))
print("CLEANUP_REMAINING", json.dumps(remaining))
