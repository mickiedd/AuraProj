"""Read-only probe: list BP_Zhengdongmen's subobjects, their classes and mesh paths.

Writes JSON rather than printing, because a commandlet's captured stdout does not carry
Python print reliably.
"""
import json
from pathlib import Path
import unreal

project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
BP = '/Game/Assets/Environment/GuangzhouLandmarks/Zhengdongmen/BP_Zhengdongmen'
bp = unreal.EditorAssetLibrary.load_asset(BP)
assert bp
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
library = unreal.SubobjectDataBlueprintFunctionLibrary
rows = []
for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
    data = subsystem.k2_find_subobject_data_from_handle(handle)
    obj = library.get_object(data)
    mesh = getattr(obj, 'static_mesh', None) if obj else None
    rows.append({'name': obj.get_name() if obj else None,
                 'class': obj.get_class().get_name() if obj else None,
                 'is_root': bool(library.is_root_component(data)),
                 'mesh': mesh.get_path_name() if mesh else None})
report = {'blueprint': bp.get_path_name(), 'count': len(rows), 'subobjects': rows}
out = project / 'Saved/Reports/Zhengdongmen/blueprint-probe.json'
out.write_text(json.dumps(report, indent=2))
unreal.log('ZHENGDONGMEN_BLUEPRINT_PROBE ' + json.dumps({'count': len(rows)}))
