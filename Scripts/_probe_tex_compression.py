"""Report the compression settings of the Guidemen textures as readable names."""
import unreal
T = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/Textures/"
for n in ("M_AgedTimber_BaseColor","M_AgedTimber_Normal","M_AgedTimber_MetallicRoughness",
          "M_AgedTimber_AO","M_WeatheredStone_BaseColor","M_GateDoor_BaseColor"):
    t = unreal.EditorAssetLibrary.load_asset(T+n)
    if not t:
        print("TEX_MISSING", n); continue
    cs = t.get_editor_property("compression_settings")
    try:
        name = cs.get_editor_property("name") if hasattr(cs, "get_editor_property") else str(cs)
    except Exception:
        name = str(cs)
    lg = t.get_editor_property("lod_group")
    print(f"TEX {n:34s} srgb={t.get_editor_property('srgb')!s:5s} "
          f"compression={name!s:28s} lodgroup={lg!s:26s} "
          f"defer={t.get_editor_property('defer_compression')!s}")
