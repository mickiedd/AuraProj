"""Generate continuous timber walls, filled arch spandrels and arch-fitting doors.
Run with the geometry venv; preserves source package and current Compact artwork.
"""
import importlib.util
import json
from pathlib import Path
import trimesh
from PIL import Image

PROJECT = Path(__file__).resolve().parents[1]
SOURCE = PROJECT / 'ContentSource/GuangzhouLandmarks/Zhengdongmen/source/build_zhengdongmen.py'
PACKAGE = PROJECT / 'Raw3DPacket/Zhengdongmen/prepared'
OUT = PROJECT / 'Saved/Reports/Zhengdongmen'

def main():
    spec=importlib.util.spec_from_file_location('zdm_builder', SOURCE)
    mod=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    builder=mod.build(0)
    scene=trimesh.Scene()
    counts={}
    for group in mod.MATERIALS:
        # Fit inside 1024 rather than forcing a square: the plaque is authored at
        # its own 4:1 aspect and squaring it would stretch the characters again.
        image=Image.open(PACKAGE / 'Textures' / f'T_ZDM_{group}_BaseColor.png').convert('RGB')
        image.thumbnail((1024,1024),Image.LANCZOS)
        mesh=builder.mesh('ZDM',group,image)
        counts[group]=len(mesh.faces)
        single=trimesh.Scene()
        name=f'SM_ZDM_LOD0_{group}'
        single.add_geometry(mesh,geom_name=name,node_name=f'LOD0_{group}')
        (PACKAGE/'Meshes'/f'SM_ZDM_{group}.glb').write_bytes(single.export(file_type='glb'))
        scene.add_geometry(mesh,geom_name=name,node_name=f'LOD0_{group}')
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'after.glb').write_bytes(scene.export(file_type='glb'))
    (OUT/'roof-tile-extents.json').write_text(json.dumps(builder.roof_extents))
    report={'groups':counts,'total_triangles':sum(counts.values()),'parts':builder.parts,
            'roof_courses':builder.roof_courses,'roof_profile':builder.roof_profile,
            'source':str(SOURCE.relative_to(PROJECT)),'arch_radius_m':2.66,'door_radius_m':2.68,
            'spring_height_m':3.4,'closed_doors':True}
    (PACKAGE/'reference-tuning.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report,indent=2))

if __name__=='__main__':main()
