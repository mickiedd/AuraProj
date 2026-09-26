"""Programmatic source validation (does NOT certify UE import, AAA fidelity, or UV2)."""
from pathlib import Path
import json,sys
from PIL import Image
import trimesh
root=Path(__file__).resolve().parents[1]
report=json.loads((root/'Documentation'/'mesh_report.json').read_text())
for lod in range(5):
    p=root/'Meshes'/f'Zhengdongmen_LOD{lod}.glb'
    scene=trimesh.load(str(p),force='scene')
    assert isinstance(scene,trimesh.Scene)
    actual=sum(len(m.faces) for m in scene.geometry.values())
    expected=report[f'LOD{lod}']['triangles'];assert actual==expected,(lod,actual,expected)
    assert actual>100
    assert all(m.faces.shape[1]==3 for m in scene.geometry.values())
    assert all(len(m.visual.uv)==len(m.vertices) for m in scene.geometry.values())
    bounds=scene.bounds
    assert bounds[1][0]-bounds[0][0] >25
    assert bounds[1][2]-bounds[0][2] >18
    print(f'LOD{lod} verified: {actual:,} triangles; {len(scene.geometry)} material meshes; bbox {bounds.tolist()}')
for material in ['Stone','Wood','RoofTile','Plaster','Iron','DoorWood','Sign']:
    for suffix in ['BaseColor','Normal','Roughness','Metallic','AO','Height']:
        path=root/'Textures'/f'T_ZDM_{material}_{suffix}_4K.png'
        with Image.open(path) as im: assert im.size==(4096,4096)
print('Verified 42 procedural 4K PNG maps, 5 GLBs, triangle-only geometry and UV0 presence.')
print('NOT VERIFIED: nonoverlapping UV2, Unreal FBX/uasset import, production-readiness or historical accuracy.')
