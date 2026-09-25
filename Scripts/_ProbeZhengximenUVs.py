"""Transient probe: is the UV channel count real, or an API artefact?

The revalidated Zhengximen meshes report 0 UV channels even though the GLBs they
were imported from carry TEXCOORD_0. Two very different explanations:

  * UE's Interchange import is dropping the UVs, or
  * get_num_uv_channels does not mean what it looks like on this build.

The way to tell them apart is to ask the same question about meshes that are known
to be textured - the Wenmingmen and GreatNorthGate landmarks render their 4K maps
correctly - and see what the same call returns for them.
"""

import json
from pathlib import Path

import unreal

REPORT = (Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
          / "Saved" / "RawModelImport" / "zhengximen-uv-probe.json")

CANDIDATES = [
    "/Game/Assets/Environment/GuangzhouLandmarks/Zhengximen/Meshes/SM_Zhengximen_GrayBrick",
    "/Game/Assets/Environment/GuangzhouLandmarks/Zhengximen/Meshes/SM_Zhengximen_AgedWood",
]

# Known-textured landmarks, discovered rather than hardcoded.
DISCOVERY_ROOTS = [
    "/Game/Assets/Environment/GuangzhouLandmarks/Wenmingmen/Meshes",
    "/Game/Assets/Environment/GuangzhouLandmarks/GreatNorthGate",
    "/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity/Meshes",
]


def api_report(mesh):
    """Every spelling of "how many UV channels", with what each returns."""
    rows = {}
    for name in ("get_num_uv_channels", "get_number_uv_channels",
                 "get_num_uv_channels_for_lod"):
        function = getattr(mesh, name, None)
        if callable(function):
            try:
                rows["mesh." + name] = int(function(0))
            except Exception as exc:  # noqa: BLE001
                rows["mesh." + name] = "{}: {}".format(type(exc).__name__, exc)
    for name in ("get_number_uv_channels", "get_num_uv_channels"):
        function = getattr(unreal.EditorStaticMeshLibrary, name, None)
        if callable(function):
            try:
                rows["lib." + name] = int(function(mesh, 0))
            except Exception as exc:  # noqa: BLE001
                rows["lib." + name] = "{}: {}".format(type(exc).__name__, exc)
    # A completely different route: how many vertices the render data has, which is
    # non-zero for any real mesh and tells us the asset loaded at all.
    try:
        rows["vertices"] = int(mesh.get_num_vertices(0))
    except Exception as exc:  # noqa: BLE001
        rows["vertices"] = str(exc)
    try:
        rows["triangles"] = int(mesh.get_num_triangles(0))
    except Exception as exc:  # noqa: BLE001
        rows["triangles"] = str(exc)
    return rows


def main():
    report = {}

    for path in CANDIDATES:
        mesh = unreal.EditorAssetLibrary.load_asset(path)
        report[path] = api_report(mesh) if isinstance(mesh, unreal.StaticMesh) \
            else "not a StaticMesh"

    known = []
    for root in DISCOVERY_ROOTS:
        for asset_path in unreal.EditorAssetLibrary.list_assets(
                root, recursive=True, include_folder=False)[:4]:
            clean = asset_path.split(".")[0]
            mesh = unreal.EditorAssetLibrary.load_asset(clean)
            if isinstance(mesh, unreal.StaticMesh):
                known.append({clean: api_report(mesh)})
    report["known_textured_landmarks"] = known

    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("ZHENGXIMEN_UV_PROBE " + json.dumps(report))
    print("ZHENGXIMEN_UV_PROBE " + json.dumps(report))


if __name__ == "__main__":
    main()
