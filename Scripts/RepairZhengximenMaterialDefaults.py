"""Repair the Zhengximen master in the live editor without reimporting geometry."""
import runpy
from pathlib import Path
import unreal

project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
module = runpy.run_path(str(project / 'Scripts/ImportZhengximenLandmark.py'))
master = module['build_master']()
module['build_instances'](master)
unreal.log('ZHENGXIMEN_MATERIAL_DEFAULTS_REPAIRED')
