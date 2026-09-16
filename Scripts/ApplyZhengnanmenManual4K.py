"""Import original 4K maps and override only the requested V2 Blueprint.

Rollback leaves the old materials/meshes intact; --rollback is handled by the
separate saved manifest-driven script. Fail on unknown material authority.
Never load/save a map, mutate shared mesh slots, or replace unrelated assets.
"""
import hashlib
import json
from pathlib import Path
import unreal

PROJECT=Path(__file__).resolve().parents[1]
SOURCE=PROJECT/"ContentSource/GuangzhouLandmarks/Zhengnanmen_Manual4K_20260916"
REPORT=PROJECT/"Saved/RawModelImport/ZhengnanmenManual4K"
DEST="/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity/Manual4K_20260916"
BP=DEST.rsplit('/',1)[0]+"/BP_GreatSouthGate_Zhengnanmen_V2_ActorAsset"
FAMILIES={"M_Stone_Aged":"Stone","M_GlazedTile_Green":"Glaze","M_Wood_RedLacquer":"RedTimber",
          "M_Wood_DarkAged":"DarkTimber","M_Gold_RidgeOrnament":"BronzeGold","M_DarkInterior":"Interior","M_Signboard_Zhengnanmen":"Plaque"}


def family(material):
    name=material.get_name()
    matches=[v for k,v in FAMILIES.items() if name==k or name.startswith(k+"_")]
    assert len(matches)==1,(name,matches)
    return matches[0]


def templates(bp):
    ss=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib=unreal.SubobjectDataBlueprintFunctionLibrary;seen=set();result=[]
    for h in ss.k2_gather_subobject_data_for_blueprint(bp):
        d=ss.k2_find_subobject_data_from_handle(h)
        if not lib.is_component(d):continue
        c=lib.get_object(d)
        if isinstance(c,unreal.StaticMeshComponent) and c.static_mesh and c.get_path_name() not in seen:
            result.append(c);seen.add(c.get_path_name())
    return result


def import_maps(manifest):
    tasks=[];records={}
    for kind,entry in manifest["materials"].items():
        records[kind]={}
        for key in ("BaseColor","Normal_DX","ORM","Height"):
            src=entry["maps"][key];path=Path(src["path"])
            assert hashlib.sha256(path.read_bytes()).hexdigest()==src["sha256"],path
            name=path.stem;target=DEST+"/Textures/"+name
            if unreal.EditorAssetLibrary.does_asset_exist(target):
                assert globals().get('RESUME_TASK_ASSETS',False),"Existing task asset; do not overwrite: "+target
                records[kind][key]=target
                continue
            task=unreal.AssetImportTask();task.set_editor_property("filename",str(path))
            task.set_editor_property("destination_path",DEST+"/Textures")
            task.set_editor_property("destination_name",name)
            task.set_editor_property("automated",True);task.set_editor_property("save",False)
            task.set_editor_property("replace_existing",False);tasks.append(task)
            records[kind][key]=target
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    maps={}
    for kind,paths in records.items():
        maps[kind]={}
        for key,path in paths.items():
            t=unreal.EditorAssetLibrary.load_asset(path);assert isinstance(t,unreal.Texture2D),path
            src=manifest["materials"][kind]["maps"][key]
            assert t.blueprint_get_size_x()==src["width"] and t.blueprint_get_size_y()==src["height"],path
            t.set_editor_property("srgb",key=="BaseColor")
            compression={"BaseColor":unreal.TextureCompressionSettings.TC_DEFAULT,"Normal_DX":unreal.TextureCompressionSettings.TC_NORMALMAP,"ORM":unreal.TextureCompressionSettings.TC_MASKS,
                         "Height":unreal.TextureCompressionSettings.TC_HALF_FLOAT}
            t.set_editor_property("compression_settings",compression[key])
            t.set_editor_property("max_texture_size",4096);t.set_editor_property("lod_bias",0)
            if key=="Normal_DX":t.set_editor_property("flip_green_channel",False)
            assert unreal.EditorAssetLibrary.save_loaded_asset(t,only_if_is_dirty=False),path
            maps[kind][key]=t
    return maps,records


def build(kind,entry,maps):
    lib=unreal.MaterialEditingLibrary
    name="M_ZNM_"+kind+"_Manual4K"+globals().get('MATERIAL_SUFFIX','')
    exists=unreal.EditorAssetLibrary.does_asset_exist(DEST+"/Materials/"+name)
    assert not exists or globals().get('RESUME_TASK_ASSETS',False), 'Existing task material'
    m=unreal.EditorAssetLibrary.load_asset(DEST+"/Materials/"+name) if exists else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,DEST+"/Materials",unreal.Material,unreal.MaterialFactoryNew())
    assert m
    lib.delete_all_material_expressions(m)
    m.set_editor_property("used_with_instanced_static_meshes",True)
    m.set_editor_property("used_with_nanite",True)
    # Source is closed solid geometry: keep normal face culling, not blanket two-sided.
    m.set_editor_property("two_sided",False)
    m.set_editor_property("shading_model",unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    def e(cls,x,y):return lib.create_material_expression(m,cls,x,y)
    def scalar(value,x,y):
        n=e(unreal.MaterialExpressionConstant,x,y);n.set_editor_property("r",value);return n
    def conn(a,out,b,pin):assert lib.connect_material_expressions(a,out,b,pin),(kind,pin)
    def prop(a,out,p):assert lib.connect_material_property(a,out,p), (kind,p)
    uv=e(unreal.MaterialExpressionTextureCoordinate,-1000,0)
    # Source UV0 is preserved. Height offsets *all* maps together, including N.
    height=e(unreal.MaterialExpressionTextureSample,-760,620)
    height.set_editor_property("texture",maps["Height"])
    height.set_editor_property("sampler_type",unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    conn(uv,"",height,"UVs")
    bump=e(unreal.MaterialExpressionBumpOffset,-480,80)
    bump.set_editor_property("height_ratio",.0015 if kind=="Stone" else (.00035 if kind=="Glaze" else .0008))
    bump.set_editor_property("reference_plane",.5)
    conn(height,"R",bump,"Height");conn(uv,"",bump,"Coordinate")
    samplers={"BaseColor":unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,"Normal_DX":unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,"ORM":unreal.MaterialSamplerType.SAMPLERTYPE_MASKS}
    samples={}
    for i,key in enumerate(samplers):
        n=e(unreal.MaterialExpressionTextureSample,-200,i*240)
        n.set_editor_property("texture",maps[key]);n.set_editor_property("sampler_type",samplers[key]);conn(bump,"",n,"UVs");samples[key]=n
    bc=samples["BaseColor"]
    if kind!="Plaque":
        random=e(unreal.MaterialExpressionPerInstanceRandom,-160,-290)
        lerp=e(unreal.MaterialExpressionLinearInterpolate,70,-220)
        conn(scalar(.92,-170,-440),"",lerp,"A");conn(scalar(1.08,-170,-370),"",lerp,"B");conn(random,"",lerp,"Alpha")
        mul=e(unreal.MaterialExpressionMultiply,340,-40);conn(bc,"RGB",mul,"A");conn(lerp,"",mul,"B");bc=mul
    prop(bc,"" if kind!="Plaque" else "RGB",unreal.MaterialProperty.MP_BASE_COLOR)
    prop(samples["Normal_DX"],"RGB",unreal.MaterialProperty.MP_NORMAL)
    for channel,p in (("R",unreal.MaterialProperty.MP_AMBIENT_OCCLUSION),("G",unreal.MaterialProperty.MP_ROUGHNESS),("B",unreal.MaterialProperty.MP_METALLIC)):
        prop(samples["ORM"],channel,p)
    prop(scalar(.5,220,900),"",unreal.MaterialProperty.MP_SPECULAR)
    # UE 5.5 hides both ClearCoat/CustomData pins from Python MaterialProperty.
    # Use physically valid default-lit glaze/lacquer roughness, not an unbound
    # ClearCoat shader. The source manifest's clearcoat value is a recommendation
    # for a later GUI-authored dual-layer master, not an applied parameter.
    lib.layout_material_expressions(m);lib.recompile_material(m)
    assert unreal.EditorAssetLibrary.save_loaded_asset(m,only_if_is_dirty=False)
    return m


def main():
    assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()==PROJECT.resolve(),"Wrong editor project"
    bp=unreal.EditorAssetLibrary.load_asset(BP);assert bp
    cs=templates(bp);before=json.loads((REPORT/"before.json").read_text())
    assert len(cs)==len(before["components"])==115
    before_names={r["name"]:r for r in before["components"]}
    # Preflight all authorities before any assets are created.
    targets=[]
    for c in cs:
        expected=before_names[c.get_name()]
        assert [c.get_material(i).get_path_name() for i in range(c.get_num_materials())]==expected["materials"],"Blueprint changed since snapshot"
        targets.append((c,[family(c.get_material(i)) for i in range(c.get_num_materials())]))
    manifest=json.loads((SOURCE/"manifest.json").read_text());assert len(manifest["materials"])==7
    maps,texrecords=import_maps(manifest)
    materials={kind:build(kind,entry,maps[kind]) for kind,entry in manifest["materials"].items()}
    # Manifest exists before the serialized Blueprint changes for recovery.
    report={"blueprint":BP,"before_snapshot":str(REPORT/"before.json"),"materials":{k:v.get_path_name() for k,v in materials.items()},"textures":texrecords,
            "source_manifest":str(SOURCE/"manifest.json"),"component_count":len(cs),"instance_count":sum(r["instances"] for r in before["components"]),
            "finish_model":"Default Lit; glaze/lacquer gloss in authored roughness. True dual-layer ClearCoat not applied because UE5.5 hides the property pins in Python.",
            "shared_meshes_changed":False,"uvs_changed":False,"collision_changed":False,"maps_saved":False,"rollback":"Restore exact per-component materials from before.json; original materials and shared meshes remain unchanged."}
    (REPORT/"apply.json").write_text(json.dumps(report,indent=2))
    try:
        for c,kinds in targets:
            c.modify()
            for i,kind in enumerate(kinds):c.set_material(i,materials[kind])
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
    except Exception:
        # Restore templates if compilation/serialization fails; never roll back a map.
        for c,_ in targets:
            for i,path in enumerate(before_names[c.get_name()]["materials"]):c.set_material(i,unreal.EditorAssetLibrary.load_asset(path))
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
        raise
    print("ZNM_MANUAL4K_APPLIED",json.dumps(report))


if __name__=="__main__":main()
