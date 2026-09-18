"""Apply the full revision-2 reference tuning delta to BP_Wuxianmen_V5_FullPBR.

Mirrors the spec in Docs/Reports/Wuxianmen-V5-FullPBR-Reference-Tuning-Spec-2026-09-17.md
and the fix recorded for the sibling 4K_Core Blueprint in
Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-reftune2-fix-20260917.json.

Scope
-----
* Geometry: 328 instance transforms (168 main tiles + 156 lower tiles + 2 main decks + 2 lower decks)
  flipped 180 deg about world X through each tier pivot.
* Ridge: HISM_004 rebound to main_ridge; two ridge bars reoriented.
* Stone: num_custom_data_floats=1; 4,213 per-instance values written.
* Materials: this script BINDS the four _RefTune2 materials assuming they exist
  (they must be created first -- see the "Material creation" section below and the
  spec doc for the exact parameter values).

Run from a normal shell (NOT the editor) after launching UnrealEditor for Aura.uproject:

    python Scripts/remote_run.py Scripts/ApplyV5WuxianmenFullPBRRefTune2.py

Engine root: UE_ENGINE_ROOT env var (defaults to C:/Git/UnrealEngine-5.5; this project
uses C:/Git/UE_5.5). The shared-editor serialization rule applies -- do not run
concurrently with another codex pass.
"""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir())
SAVED = PROJECT_ROOT / "Saved/RawModelImport/V5"
BLUEPRINT_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_FullPBR/BP_Wuxianmen_V5_FullPBR"
PACKAGE = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_FullPBR"
MEShes_REF = f"{PACKAGE}/Meshes/ReferenceTuned20260917"

# Roof flip pivots (cm) -- per reftune2 fix spec.
TILE_PIVOTS = {"main": 1423.5, "lower": 1251.5}
DECK_PIVOTS = {"main": 1403.5, "lower": 1236.5}

# Ridge: index -> (tier, translation_cm, rotator_keyword, scale, x_span_cm)
#   'rotator_keyword' uses the keyword form (pitch=, yaw=, roll=) to round-trip
#   exactly. See unreal-asset-reference-tuning skill, "Unreal Python API contract".
RIDGE_FIX = {
    4: {
        "tier": "lower",
        "translation": unreal.Vector(0.0, 0.0, 1338.0),
        "rotator": unreal.Rotator(pitch=-90.0, yaw=-90.0, roll=-90.0),
        "scale": unreal.Vector(0.42, 0.42, 21.0),
    },
    9: {
        "tier": "main",
        "translation": unreal.Vector(0.0, 0.0, 1572.0),
        "rotator": unreal.Rotator(pitch=-90.0, yaw=-10.0249918334942, roll=-169.97501830037848),
        "scale": unreal.Vector(0.42, 0.42, 22.5),
    },
}

# Component -> new material bindings (post-fix).
MATERIAL_BINDINGS = {
    "HISM_000_Wuxianmen_Plaque_GEN_VARIABLE": f"{PACKAGE}/Materials/M_Plaque_Wuxianmen_RefTune2.M_Plaque_Wuxianmen_RefTune2",
    "HISM_002_lower_tile_1_077_GEN_VARIABLE": f"{PACKAGE}/Materials/M_RoofTile_ReferenceTuned_RefTune2.M_RoofTile_ReferenceTuned_RefTune2",
    "HISM_003_plaster_000016_GEN_VARIABLE": f"{PACKAGE}/Materials/M_Plaster_ReferenceTuned_RefTune2.M_Plaster_ReferenceTuned_RefTune2",
    "HISM_004_ridge_000010_GEN_VARIABLE": f"{PACKAGE}/Materials/M_RoofTile_ReferenceTuned_RefTune2.M_RoofTile_ReferenceTuned_RefTune2",
    "HISM_005_stone_004213_GEN_VARIABLE": f"{PACKAGE}/Materials/M_Stone_ReferenceTuned_RefTune2.M_Stone_ReferenceTuned_RefTune2",
}


def _hash_unit01(index: int, salt: int) -> float:
    h = hashlib.md5(f"{index}-{salt}".encode()).digest()
    return (int.from_bytes(h[:4], "big") % 1_000_000) / 1_000_000.0


def _stone_custom_value(index: int) -> float:
    # Mirror the reftune2 pass: uv_offset in [0,1) + tone_variation in [-0.15, +0.15].
    uv = _hash_unit01(index, salt=0)
    tone = (_hash_unit01(index, salt=0x5F3759DF) * 0.30) - 0.15
    # Pack as a single float. The reftune2 material reads this via a custom HLSL
    # unpacking node; for FullPBR the executor must author the equivalent node
    # when creating M_Stone_ReferenceTuned_RefTune2.
    return uv + tone


def _load_blueprint():
    bp = unreal.EditorAssetLibrary.load_asset(BLUEPRINT_PATH)
    assert isinstance(bp, unreal.Blueprint), f"{BLUEPRINT_PATH} is not a Blueprint"
    return bp


def _gather_hism_components(blueprint):
    """Return the seven HISMs as {component_name: HISMInstance}."""
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    out = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        obj = library.get_object(data)
        if obj is None:
            continue
        # Filter to HISM components only -- skip the root, lights, etc.
        if "HISM_" in obj.get_name():
            out[obj.get_name()] = obj
    return out


def _instance_world_transform(hism, index):
    return hims.get_instance_transform(index, world_space=False, mark_dirty=False)


def _z_mirror_about_x(t: unreal.Transform, pivot_z_cm: float) -> unreal.Transform:
    """Apply 180 deg rotation about world X axis through (0, 0, pivot_z_cm).

    Translation: translate to pivot, mirror (x, -y, -z), translate back.
    Rotation: multiply the original quaternion on the right by q180x = (1,0,0,0)
    which represents a 180 deg X rotation. Per the skill's API contract,
    MathLibrary.multiply_quat_quat(A, B) applies B first then A, so the call
    should be `multiply_quat(Q_orig, q180x)` to pre-rotate by 180 about X.
    """
    p = t.translation
    v = unreal.Vector(p.x, p.y, p.z - pivot_z_cm)
    v_mirror = unreal.Vector(v.x, -v.y, -v.z)
    new_translation = unreal.Vector(v_mirror.x, v_mirror.y, v_mirror.z + pivot_z_cm)
    q_orig = t.rotation
    # q180x = (sin(pi/2), 0, 0, cos(pi/2)) -- but for a 180 deg rotation the
    # quaternion is (1, 0, 0, 0). Use QuatMake with the four components.
    q180x = unreal.Quat(1.0, 0.0, 0.0, 0.0)
    q_new = unreal.MathLibrary.multiply_quat_quat(q_orig, q180x)
    return unreal.Transform(new_translation, q_new, t.scale3d)


def audit(blueprint, hisms):
    """Write the baseline JSON and assert the invariant counts."""
    baseline = {"blueprint": BLUEPRINT_PATH, "components": {}, "instance_total": 0}
    for name, hims_obj in hisms.items():
        count = hims_obj.get_instance_count()
        mesh_path = hims_obj.static_mesh.get_path_name() if hims_obj.static_mesh else None
        mats = [m.get_path_name() for m in hims_obj.get_materials() if m is not None]
        baseline["components"][name] = {
            "instance_count": count, "mesh": mesh_path, "materials": mats,
            "visible": hims_obj.is_visible(), "collision": hims_obj.get_collision_profile_name(),
        }
        baseline["instance_total"] += count
    SAVED.mkdir(parents=True, exist_ok=True)
    out = SAVED / "Wuxianmen_V5_FullPBR-reftune2-baseline-20260917.json"
    out.write_text(json.dumps(baseline, indent=2), encoding="utf-8")
    assert len(hisms) == 7, f"expected 7 HISMs, found {len(hisms)}"
    assert baseline["instance_total"] == 5103, f"expected 5103 instances, found {baseline['instance_total']}"
    return baseline


def apply_roof_flip(hisms):
    """Flip 168 main tiles + 156 lower tiles + 2 main decks + 2 lower decks."""
    tile_hism = hisms["HISM_002_lower_tile_1_077_GEN_VARIABLE"]
    wood_hism = hisms["HISM_006_wood_000451_GEN_VARIABLE"]

    changed = {"tile:main": 0, "tile:lower": 0, "wooddeck:main": 0, "wooddeck:lower": 0}
    for idx in range(tile_hism.get_instance_count()):
        t = _instance_world_transform(tile_hism, idx)
        # Tier from the instance's local y half: y >= 0 -> main.
        tier = "main" if t.translation.y >= 0.0 else "lower"
        new_t = _z_mirror_about_x(t, TILE_PIVOTS[tier])
        # Verify: transformed box (approx, via mirrored translation) matches mirror rule.
        # The full box-equality check requires get_bounds(); left for the validation pass.
        tile_hism.update_instance_transform(idx, new_t, world_space=False, teleport=True)
        changed[f"tile:{tier}"] += 1

    for idx in range(wood_hism.get_instance_count()):
        t = _instance_world_transform(wood_hism, idx)
        tier = "main" if t.translation.y >= 0.0 else "lower"
        new_t = _z_mirror_about_x(t, DECK_PIVOTS[tier])
        wood_hism.update_instance_transform(idx, new_t, world_space=False, teleport=True)
        changed[f"wooddeck:{tier}"] += 1

    assert changed["tile:main"] == 168, changed
    assert changed["tile:lower"] == 156, changed
    assert changed["wooddeck:main"] == 2, changed
    assert changed["wooddeck:lower"] == 2, changed
    return changed


def apply_ridge_rebind_and_reorient(hisms):
    """Rebind HISM_004 to main_ridge and set the two per-tier transforms."""
    ridge_hism = hisms["HISM_004_ridge_000010_GEN_VARIABLE"]
    ridge_mesh = unreal.EditorAssetLibrary.load_asset(f"{MEShes_REF}/main_ridge")
    assert ridge_mesh is not None, "main_ridge mesh missing in ReferenceTuned20260917"
    ridge_hism.set_static_mesh(ridge_mesh)

    # The 10 ridge instances are indexed 0..9. Per the fix spec, indices 4 (lower)
    # and 9 (main) carry the actual ridge bars; the other 8 are end caps and hip
    # covers and keep their authored orientation.
    for idx, spec in RIDGE_FIX.items():
        t = unreal.Transform(spec["translation"], spec["rotator"], spec["scale"])
        ridge_hism.update_instance_transform(idx, t, world_space=False, teleport=True)
    return {"rebound_mesh": "main_ridge", "reoriented_indices": list(RIDGE_FIX.keys())}


def apply_stone_custom_data(hisms):
    """Assign num_custom_data_floats=1 and 4,213 per-instance values."""
    stone_hism = hisms["HISM_005_stone_004213_GEN_VARIABLE"]
    n = stone_hism.get_instance_count()
    assert n == 4213, f"expected 4213 stone instances, found {n}"
    stone_hism.set_num_custom_data_floats(1)
    for idx in range(n):
        stone_hism.set_custom_data_value(idx, 0, _stone_custom_value(idx))
    return {"instances": n, "num_custom_data_floats": 1}


def apply_material_bindings(hisms):
    """Bind the four _RefTune2 materials assuming they already exist.

    Material creation is a separate graph-editing step -- see the spec doc for the
    exact parameter values. This function only verifies each material asset exists
    and binds it; if any are missing it raises with a remediation pointer.
    """
    bound = {}
    for comp_name, mat_path in MATERIAL_BINDINGS.items():
        mat = unreal.EditorAssetLibrary.load_asset(mat_path)
        if mat is None:
            raise RuntimeError(
                f"Missing material {mat_path!r} required for {comp_name}. "
                "Create it first per the spec doc (graph editing, not an instance override) "
                "then re-run this script."
            )
        hims[comp_name].set_material(0, mat)
        bound[comp_name] = mat_path
    return bound


PLAQUE_PNG = (
    PROJECT_ROOT
    / "ContentSource/GuangzhouLandmarks/V5ReferenceTuning20260917"
    / "Wuxianmen_V5_FullPBR/Textures/Wuxianmen_Plaque_BaseColor_2K.png"
)
PLAQUE_DEST = f"{PACKAGE}/Textures/Wuxianmen_Plaque_BaseColor_2K.Wuxianmen_Plaque_BaseColor_2K"


def import_plaque_texture():
    """Import the generated plaque PNG as a UE texture asset.

    Run before material binding so the plaque material (authored separately via
    graph editing) can reference it. Idempotent: skips if the asset already exists.
    Source PNG is generated by Scripts/GenerateWuxianmenFullPBRPlaqueTexture.py.
    """
    assert PLAQUE_PNG.exists(), f"missing plaque source PNG: {PLAQUE_PNG}"
    if unreal.EditorAssetLibrary.does_asset_exist(PLAQUE_DEST):
        return unreal.EditorAssetLibrary.load_asset(PLAQUE_DEST)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(PLAQUE_PNG))
    task.set_editor_property("destination_path", f"{PACKAGE}/Textures")
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = unreal.EditorAssetLibrary.load_asset(PLAQUE_DEST)
    assert tex is not None, f"plaque texture import failed for {PLAQUE_PNG}"
    return tex


def main():
    # Never save /Game/Maps/Login; record it.
    dirty_before = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    print(f"DirtyMapsBefore={json.dumps([str(p) for p in dirty_before])}")

    blueprint = _load_blueprint()
    hisms = _gather_hism_components(blueprint)
    baseline = audit(blueprint, hisms)
    print(f"BASELINE components={len(hisms)} instances={baseline['instance_total']}")

    roof = apply_roof_flip(hisms)
    print(f"ROOF_FLIP {json.dumps(roof)}")

    ridge = apply_ridge_rebind_and_reorient(hisms)
    print(f"RIDGE {json.dumps(ridge)}")

    stone = apply_stone_custom_data(hisms)
    print(f"STONE_CUSTOM_DATA {json.dumps(stone)}")

    plaque_tex = import_plaque_texture()
    print(f"PLAQUE_TEXTURE {plaque_tex.get_path_name()}")

    bound = apply_material_bindings(hisms)
    print(f"MATERIAL_BINDINGS {len(bound)} bound")

    # Compile + save the Blueprint so the reftune2 changes persist.
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)

    dirty_after = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    # Sanity: /Game/Maps/Login should still be dirty but no new dirty maps should
    # appear beyond it. If a new one shows up the script must be revisited.
    print(f"DirtyMapsAfter={json.dumps([str(p) for p in dirty_after])}")

    summary = {
        "passed": True,
        "blueprint": BLUEPRINT_PATH,
        "roof_flip": roof,
        "ridge": ridge,
        "stone_custom_data": stone,
        "plaque_texture": plaque_tex.get_path_name(),
        "material_bindings": bound,
        "dirty_maps_before": [str(p) for p in dirty_before],
        "dirty_maps_after": [str(p) for p in dirty_after],
    }
    out = SAVED / "Wuxianmen_V5_FullPBR-reftune2-fix-20260917.json"
    out.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(f"WUXIANMEN_V5_FULLPBR_REFTUNE2_APPLIED {json.dumps(summary)}")


if __name__ == "__main__":
    main()
