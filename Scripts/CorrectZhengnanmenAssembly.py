"""Remove the complete, incompatible UltraAAA overlay from the V2 gate.

The 114 packed source components are the reference-shaped model authority.
UltraAAA's generated mesh contains another entire gate at different floor
heights, so it cannot be mounted as additive detail. Save only this Blueprint;
preserve source meshes, 4K overrides, instances, collision and open maps.
"""
import hashlib
import json
from pathlib import Path
import unreal

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Saved/RawModelImport/ZhengnanmenAssemblyCorrection'
BP='/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity/BP_GreatSouthGate_Zhengnanmen_V2_ActorAsset'

def templates(bp):
    ss=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib=unreal.SubobjectDataBlueprintFunctionLibrary
    seen=set();rows=[]
    for h in ss.k2_gather_subobject_data_for_blueprint(bp):
        d=ss.k2_find_subobject_data_from_handle(h)
        if not lib.is_component(d):continue
        c=lib.get_object(d)
        if not isinstance(c,unreal.StaticMeshComponent) or not c.static_mesh or c.get_path_name() in seen:continue
        seen.add(c.get_path_name());rows.append((h,c))
    return ss,rows

def snapshot(rows):
    return {c.get_name():dict(mesh=c.static_mesh.get_path_name(),instances=c.get_instance_count() if isinstance(c,unreal.InstancedStaticMeshComponent) else 1,
        materials=[c.get_material(i).get_path_name() if c.get_material(i) else '' for i in range(c.get_num_materials())],
        collision=str(c.get_collision_enabled()),visible=c.get_editor_property('visible'),hidden=c.get_editor_property('hidden_in_game'),
        transform={key:[float(getattr(c.get_editor_property(key),a)) for a in axes] for key,axes in (('relative_location','xyz'),('relative_rotation',('pitch','yaw','roll')),('relative_scale3d','xyz'))}) for h,c in rows}

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    bp=unreal.EditorAssetLibrary.load_asset(BP);assert bp and bp.generated_class()
    ss,rows=templates(bp)
    before=snapshot(rows)
    overlays=[(h,c) for h,c in rows if c.get_name().startswith('UltraAAA_HeroGeometry')]
    expected={k:v for k,v in before.items() if not k.startswith('UltraAAA_HeroGeometry')}
    assert len(expected)==114 and sum(r['instances'] for r in expected.values())==12387
    if not overlays and (OUT/'apply.json').exists():
        assert expected==json.loads((OUT/'apply.json').read_text())['after']
        print('ZNM_ASSEMBLY_ALREADY_CORRECTED; original evidence preserved')
        return
    root=next(h for h in ss.k2_gather_subobject_data_for_blueprint(bp) if unreal.SubobjectDataBlueprintFunctionLibrary.is_root_component(ss.k2_find_subobject_data_from_handle(h)))
    removed=[]
    for h,c in overlays:
        removed.append(c.get_name())
        count=ss.delete_subobject(root,h,bp)
        assert count==1,(removed,count)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    after=snapshot(templates(bp)[1]);assert after==expected,'Packed source data changed'
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
    report=dict(passed=True,blueprint=BP,removed=removed,before=before,after=after,
        reference_authority='Original packed V2 three-tier upturned roof assembly',component_count=len(after),instance_count=sum(r['instances'] for r in after.values()),
        source_materials_transforms_collision_preserved=True,maps_saved=False)
    (OUT/'apply.json').write_text(json.dumps(report,indent=2))
    print('ZNM_ASSEMBLY_CORRECTED',removed,len(after),report['instance_count'])

if __name__=='__main__':main()
