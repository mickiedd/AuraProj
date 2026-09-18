"""Inspect the Guidemen _ReferenceTuned material graphs before adding weathering."""
import unreal

MEL = unreal.MaterialEditingLibrary
BASE = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/Materials/"
for name in ("M_WeatheredStone_4K_ReferenceTuned", "M_GrayClayTile_2K_ReferenceTuned",
             "M_AgedTimber_4K_ReferenceTuned", "M_FadedRedWood_ReferenceTuned"):
    mat = unreal.EditorAssetLibrary.load_asset(BASE + name)
    if not mat:
        print("MAT_MISSING", name)
        continue
    try:
        exprs = MEL.get_material_expressions(mat)
    except Exception:
        exprs = []
    kinds = {}
    for e in exprs:
        kinds[e.get_class().get_name()] = kinds.get(e.get_class().get_name(), 0) + 1
    used = [t.get_path_name().split("/")[-1].split(".")[0] for t in MEL.get_used_textures(mat)]
    # which properties are connected
    try:
        inputs = [p.get_editor_property("input_name") for p in MEL.get_material_inputs(mat)]
    except Exception:
        inputs = []
    print("MAT", name, "| expressions", len(exprs), "| kinds", kinds)
    print("    used_textures", used)
    print("    two_sided", mat.get_editor_property("two_sided"),
          "nanite", mat.get_editor_property("used_with_nanite"))
