# Zhengximen / Great West Gate UE5 import helper
# Tested as a conservative source-import recipe for UE 5.x Python environments.
# Run in Unreal Editor: Tools > Execute Python Script.
import unreal, os
ROOT=os.path.abspath(os.path.join(os.path.dirname(__file__),'..'))
DEST='/Game/Zhengximen'
MESH=os.path.join(ROOT,'Meshes'); TEX=os.path.join(ROOT,'Textures')
asset_tools=unreal.AssetToolsHelpers.get_asset_tools()

def import_fbx(path,dest,combine=True):
    task=unreal.AssetImportTask(); task.filename=path; task.destination_path=dest
    task.automated=True; task.save=True; task.replace_existing=True
    ui=unreal.FbxImportUI(); ui.import_mesh=True; ui.import_as_skeletal=False
    ui.import_materials=False; ui.import_textures=False
    try:
        ui.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
        ui.static_mesh_import_data.combine_meshes=combine
        ui.static_mesh_import_data.generate_lightmap_u_vs=False
        ui.static_mesh_import_data.auto_generate_collision=False
        ui.static_mesh_import_data.remove_degenerates=True
    except Exception as e: unreal.log_warning('FBX option compatibility: '+str(e))
    task.options=ui; asset_tools.import_asset_tasks([task]); return task.imported_object_paths

def import_texture(path):
    task=unreal.AssetImportTask(); task.filename=path; task.destination_path=DEST+'/Textures'
    task.automated=True; task.save=True; task.replace_existing=True
    asset_tools.import_asset_tasks([task]); return task.imported_object_paths

# 1) Import all PBR textures
for fn in sorted(os.listdir(TEX)):
    if fn.lower().endswith('.png'): import_texture(os.path.join(TEX,fn))

# 2) Import LOD0 as one combined Static Mesh. LOD0 FBX includes UV0, lightmap UV1, UCX proxies, sockets.
paths=import_fbx(os.path.join(MESH,'SM_Zhengximen_LOD0.fbx'),DEST+'/Meshes',True)
sm=None
for p in paths:
    a=unreal.load_asset(p)
    if isinstance(a,unreal.StaticMesh): sm=a; break
if sm:
    try:
        ns=sm.get_editor_property('nanite_settings'); ns.enabled=True; sm.set_editor_property('nanite_settings',ns)
    except Exception as e: unreal.log_warning('Nanite setting: '+str(e))
    # Import authored LODs. API name is available in many UE5 builds; otherwise use Static Mesh Editor > LOD Import.
    for i in range(1,5):
        try: unreal.EditorStaticMeshLibrary.import_lod(sm,i,os.path.join(MESH,f'SM_Zhengximen_LOD{i}.fbx'))
        except Exception as e: unreal.log_warning(f'LOD{i} import: {e}')
    unreal.EditorAssetLibrary.save_loaded_asset(sm)
else:
    unreal.log_warning('No StaticMesh path was returned. Import SM_Zhengximen_LOD0.fbx manually with Combine Meshes ON.')

unreal.log('Zhengximen mesh/textures imported. Run Build_Zhengximen_Materials_UE5.py next.')
