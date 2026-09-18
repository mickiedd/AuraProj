"""Inspect the imported Guidemen texture assets: are BaseColor maps actually normal maps?"""
import unreal
T = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/Textures/"
names = ["M_AgedTimber_BaseColor","M_AgedTimber_Normal","M_GateDoor_BaseColor","M_GateDoor_Normal",
         "M_WeatheredStone_BaseColor","M_WeatheredStone_Normal","M_GrayClayTile_BaseColor",
         "M_GrayClayTile_Normal","M_AgedPlaster_BaseColor","M_AgedPlaster_Normal"]
for n in names:
    t = unreal.EditorAssetLibrary.load_asset(T+n)
    if not t:
        print("TEX_MISSING", n); continue
    props = {}
    for p in ("srgb","compression_settings","lod_group","never_stream","compression_no_alpha",
              "texture_group","mip_gen_settings"):
        try: props[p] = str(t.get_editor_property(p))
        except Exception as e: props[p] = "n/a"
    print(f"TEX {n:32s} srgb={props['srgb']:5s} comp={props['compression_settings'][:22]:24s} "
          f"lodgroup={props['lod_group'][:22]:24s} size={t.blueprint_get_size_x()}x{t.blueprint_get_size_y()}")
