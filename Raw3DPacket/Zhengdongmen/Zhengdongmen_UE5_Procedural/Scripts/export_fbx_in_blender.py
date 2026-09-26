"""OPTIONAL user-side conversion: Blender 4.x required; not executed by package creator.
Blender -b -P Scripts/export_fbx_in_blender.py -- [--lightmap-uv]
Input:  Meshes/Zhengdongmen_LOD0.glb ... LOD4.glb
Output: Meshes/FBX/Zhengdongmen_LOD0.fbx ... LOD4.fbx, with UV2 when enabled.
"""
import bpy,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]; OUT=ROOT/'Meshes'/'FBX'; OUT.mkdir(parents=True,exist_ok=True)
make_uv2='--lightmap-uv' in sys.argv
for level in range(5):
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.gltf(filepath=str(ROOT/'Meshes'/f'Zhengdongmen_LOD{level}.glb'))
    objects=[ob for ob in bpy.context.scene.objects if ob.type=='MESH']
    bpy.ops.object.select_all(action='DESELECT')
    for ob in objects:ob.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
    if len(objects)>1:bpy.ops.object.join()
    obj=bpy.context.view_layer.objects.active;obj.name=f'SM_ZDM_LOD{level}'
    if make_uv2:
        bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
        obj.data.uv_layers.new(name='LightmapUV');obj.data.uv_layers.active=obj.data.uv_layers['LightmapUV']
        bpy.ops.uv.smart_project(island_margin=0.01)
        bpy.ops.object.mode_set(mode='OBJECT');obj.data.uv_layers.active_index=0
    # Blender GLB importer converts glTF Y-up to Blender Z-up.
    bpy.ops.export_scene.fbx(filepath=str(OUT/f'Zhengdongmen_LOD{level}.fbx'),use_selection=True,
        apply_unit_scale=True,axis_forward='-Y',axis_up='Z',add_leaf_bones=False,path_mode='AUTO')
    print('FBX exported:',level)
