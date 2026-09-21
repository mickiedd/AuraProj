"""Re-measure the repaired meshes after import, to prove the repair survived.

The repair was applied to OBJ files on disk, then imported. Both steps can change
the geometry, so the only meaningful check is to export the imported asset again
and measure it: duplicates, degenerates, inward solids and interior winding defects
must all be zero.

Writes OBJs into a separate folder so the analysis covers only the repaired assets,
and reuses the same analyzer.
"""

import json
import os
from pathlib import Path

import unreal

OUT_DIR = Path("C:/Git/AuraProj/Saved/MeshAudit/repaired_obj")
IMPORT_REPORT = Path("C:/Git/AuraProj/Saved/MeshAudit/import-report.json")


def main():
    imports = json.loads(IMPORT_REPORT.read_text(encoding="utf-8"))
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    exported = []
    for name, entry in sorted(imports.items()):
        asset_path = entry.get("asset")
        if not asset_path:
            continue
        mesh = unreal.EditorAssetLibrary.load_asset(asset_path)
        if mesh is None:
            exported.append({"mesh": name, "error": "asset missing"})
            continue
        filename = str(OUT_DIR / (name + ".obj"))
        task = unreal.AssetExportTask()
        task.set_editor_property("object", mesh)
        task.set_editor_property("filename", filename)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_identical", True)
        task.set_editor_property("prompt", False)
        try:
            unreal.Exporter.run_asset_export_task(task)
        except Exception as exc:
            exported.append({"mesh": name, "error": "export failed: {}".format(exc)})
            continue
        exported.append({"mesh": name, "asset": asset_path,
                         "obj": filename, "bytes": os.path.getsize(filename)
                         if os.path.exists(filename) else 0})
        print("REAUDIT_EXPORT", json.dumps(exported[-1]))

    unreal.log("REAUDIT_EXPORTED " + json.dumps(exported))
    print("REAUDIT_EXPORTED", len(exported))


if __name__ == "__main__":
    main()
