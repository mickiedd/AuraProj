"""Dump Xiaobeimen V3 mesh geometry (positions, triangles, UVs) to JSON.

The V3 asset has no procedural source, so the geometry has to be read out of the engine to
be reasoned about: the arch's inner profile to fit a door to it, the plaque's current
height, and the wall's UV range to judge its block scale. `StaticMeshDescription` is
per-element — `get_vertex_position`, `get_triangle_vertex_instances`,
`get_vertex_instance_uv` — so this walks it explicitly.
"""
import json
from pathlib import Path
import unreal

ROOT = '/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_AAA_V3'
PARTS = ('Arch_Voussoirs__M_StoneWall', 'Wooden_Doors__M_AgedWood',
         'Plaque_Body__M_StoneWall', 'Plaque_Face__M_Plaque',
         'Wall_Main__M_StoneWall', 'Wall_ArchSpandrel__M_StoneWall')

project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
out_dir = project / 'Saved/Reports/Xiaobeimen/geometry'
out_dir.mkdir(parents=True, exist_ok=True)

summary = {}
for part in PARTS:
    mesh = unreal.EditorAssetLibrary.load_asset(
        '{}/Meshes/Xiaobeimen_HP/{}'.format(ROOT, part))
    if not isinstance(mesh, unreal.StaticMesh):
        summary[part] = {'error': 'not a StaticMesh'}
        continue
    md = mesh.get_static_mesh_description(0)
    vertex_count = md.get_vertex_count()
    positions = []
    for index in range(vertex_count):
        v = md.get_vertex_position(unreal.VertexID(index))
        positions.append([round(float(v.x), 4), round(float(v.y), 4), round(float(v.z), 4)])

    triangle_count = md.get_triangle_count()
    uvs = []
    for index in range(triangle_count):
        tri = unreal.TriangleID(index)
        tri_uvs = []
        for corner in range(3):
            uv = md.get_vertex_instance_uv(md.get_triangle_vertex_instance(tri, corner))
            tri_uvs.append([round(float(uv.x), 5), round(float(uv.y), 5)])
        uvs.append(tri_uvs)

    # Triangle indices are deliberately not recorded: VertexID exposes no id accessor in
    # this build, and every measurement this job needs — the arch's inner profile, the
    # plaque's height, the wall's UV range — comes from the positions and the UVs alone.
    payload = {'part': part, 'vertex_count': vertex_count,
               'triangle_count': triangle_count, 'positions': positions, 'uvs': uvs}
    (out_dir / (part + '.json')).write_text(json.dumps(payload))
    u_flat = [uv[0] for tri in uvs for uv in tri]
    v_flat = [uv[1] for tri in uvs for uv in tri]
    summary[part] = {'triangles': triangle_count, 'vertices': vertex_count,
                     'uv_u': [round(min(u_flat), 4), round(max(u_flat), 4)] if u_flat else None,
                     'uv_v': [round(min(v_flat), 4), round(max(v_flat), 4)] if v_flat else None,
                     'file': part + '.json'}

report = {'parts': summary, 'out': str(out_dir)}
(project / 'Saved/Reports/Xiaobeimen/geometry-summary.json').write_text(
    json.dumps(report, indent=2))
unreal.log('XIAOBEIMEN_GEOMETRY_DUMPED ' + json.dumps(summary))
