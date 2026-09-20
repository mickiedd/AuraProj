"""Audit every Guangzhou landmark material for the Nanite blend-mode crash.

The editor log records a fatal assertion on 2026-09-15:

    Assertion failed: Nanite::IsSupportedBlendMode(ShadingMaterial)

Nanite cannot render a material whose blend mode or shading model it does not support, and the
engine asserts rather than degrading. This scans every material under the GuangzhouLandmarks
folders and reports blend mode, shading model, two-sided and Nanite flags, flagging any material
that has Nanite enabled with an unsupported combination.

Read-only.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
OUTPUT = PROJECT / "Saved/RawModelImport/nanite-material-audit.json"
ROOT = "/Game/Assets/Environment/GuangzhouLandmarks"

# Nanite supports the opaque and masked blend modes; translucent, additive and modulated are not.
SUPPORTED_BLEND = {"BLEND_OPAQUE", "BLEND_MASKED", "BLEND_ALPHA_COMPOSITE", "BLEND_ALPHA_HOLDOUT"}
# Nanite supports Default Lit and Unlit. Everything else asserts.
SUPPORTED_SHADING = {"MSM_DEFAULT_LIT", "MSM_UNLIT"}


def main():
    paths = unreal.EditorAssetLibrary.list_assets(ROOT, recursive=True, include_folder=False)
    materials = [p for p in paths if "/Materials/" in p or p.rsplit("/", 1)[-1].startswith("M_")]
    report = {"root": ROOT, "materials": [], "unsafe": [], "map_saved": False}
    for path in sorted(set(materials)):
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if not isinstance(asset, unreal.Material):
            continue
        if isinstance(asset, unreal.MaterialInstance):
            continue
        entry = {"material": path}
        for prop in ("blend_mode", "shading_model", "two_sided", "used_with_nanite",
                     "translucency_lighting_mode"):
            try:
                value = asset.get_editor_property(prop)
                entry[prop] = str(value).split(".")[-1] if value is not None else None
            except Exception:
                entry[prop] = None
        report["materials"].append(entry)
        blend = entry.get("blend_mode")
        shading = entry.get("shading_model")
        if entry.get("used_with_nanite") == "True":
            if blend not in SUPPORTED_BLEND or shading not in SUPPORTED_SHADING:
                report["unsafe"].append(entry)
                print(f"NANITE_UNSAFE {path.split('/')[-1][:44]:46s} blend={blend} shading={shading}")
    print("NANITE_AUDIT materials", len(report["materials"]), "unsafe", len(report["unsafe"]))
    for entry in report["materials"][:12]:
        print(f"  {entry['material'].split('/')[-1][:40]:42s} nanite={entry.get('used_with_nanite')} "
              f"blend={entry.get('blend_mode')} shading={entry.get('shading_model')}")
    OUTPUT.write_text(json.dumps(report, indent=1), encoding="utf-8")
    print("NANITE_AUDIT_OUTPUT", OUTPUT)


if __name__ == "__main__":
    main()
