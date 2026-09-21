"""Inventory the GuangzhouLandmarks folder: which landmarks are already
independent Blueprints, and which are still bare StaticMeshes.

Read-only. Enumerates every asset under the landmark root, classifies it, and
for the two interesting classes prints the structure a wrapper would need to
reproduce:

  Blueprint    -> generated class, component count, per-component mesh path,
                  instance count (HISM/ISM) and mobility
  StaticMesh   -> bounds, material slot count and slot material paths

Nothing is spawned, saved or modified.
"""

import json

import unreal

ROOT = "/Game/Assets/Environment/GuangzhouLandmarks"

# Textures / material graphs are not landmarks; they are dependencies of one.
IGNORED_SUFFIXES = ("_BaseColor", "_Normal", "_NormalDX", "_ORM",
                    "_MetallicRoughness", "_Roughness", "_AO", "_NormalMap")


def is_ignorable(name, asset_class):
    cls = asset_class.get_name()
    if cls in ("Texture2D", "Material", "MaterialInstanceConstant",
               "MaterialFunction", "World", "PhysicsAsset"):
        return True
    return any(name.endswith(suffix) for suffix in IGNORED_SUFFIXES)


def list_assets():
    paths = []
    for folder in unreal.EditorAssetLibrary.list_assets(ROOT, recursive=True, include_folder=False):
        paths.append(folder.split(".")[0])
    return sorted(set(paths))


def component_records(blueprint):
    """Describe the Blueprint's component tree: one row per component."""
    rows = []
    try:
        subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    except Exception:
        subsystem = None
    generated = blueprint.generated_class()
    if generated is None:
        return rows
    cdo = unreal.get_default_object(generated)
    if cdo is None:
        return rows
    for component in cdo.get_components_by_class(unreal.ActorComponent):
        row = {
            "class": component.get_class().get_name(),
            "name": component.get_name(),
        }
        for attr in ("static_mesh", "mobility", "relative_location", "relative_scale3d"):
            try:
                value = component.get_editor_property(attr)
            except Exception:
                continue
            if attr == "static_mesh":
                row["mesh"] = value.get_path_name() if value else None
            elif attr == "mobility":
                row["mobility"] = str(value)
            elif attr == "relative_location":
                row["rel_loc"] = [round(float(value.x), 2), round(float(value.y), 2),
                                  round(float(value.z), 2)]
            elif attr == "relative_scale3d":
                row["rel_scale"] = [round(float(value.x), 4), round(float(value.y), 4),
                                    round(float(value.z), 4)]
        for attr, key in (("instance_start_cull_distance", "cull"),
                          ("num_custom_data_floats", "custom_floats")):
            try:
                row[key] = component.get_editor_property(attr)
            except Exception:
                pass
        for attr, key in (("get_instance_count", "instances"),
                          ("get_num_instances", "instances")):
            if hasattr(component, attr):
                try:
                    row["instances"] = int(getattr(component, attr)())
                    break
                except Exception:
                    pass
        rows.append(row)
    return rows


def mesh_record(mesh):
    bounds = mesh.get_bounds()
    origin = bounds.origin
    extent = bounds.box_extent
    slots = []
    try:
        for static_material in mesh.get_editor_property("static_materials"):
            material = static_material.get_editor_property("material_interface")
            slots.append(material.get_path_name() if material else None)
    except Exception as exc:
        slots.append("unreadable: {}".format(exc))
    return {
        "origin_cm": [round(float(origin.x), 2), round(float(origin.y), 2), round(float(origin.z), 2)],
        "size_cm": [round(float(extent.x) * 2, 2), round(float(extent.y) * 2, 2),
                    round(float(extent.z) * 2, 2)],
        "material_slots": slots,
    }


def main():
    report = {"root": ROOT, "blueprints": [], "static_meshes": [], "other": []}
    for path in list_assets():
        name = path.rsplit("/", 1)[-1]
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if asset is None:
            report["other"].append({"path": path, "error": "load failed"})
            continue
        asset_class = asset.get_class()
        if is_ignorable(name, asset_class):
            continue
        if isinstance(asset, unreal.Blueprint):
            entry = {
                "path": path,
                "parent_class": blueprint_parent(asset),
                "generated_class": (asset.generated_class().get_name()
                                    if asset.generated_class() else None),
                "components": component_records(asset),
            }
            entry["component_count"] = len(entry["components"])
            report["blueprints"].append(entry)
        elif isinstance(asset, unreal.StaticMesh):
            entry = {"path": path}
            entry.update(mesh_record(asset))
            report["static_meshes"].append(entry)
        else:
            report["other"].append({"path": path, "class": asset_class.get_name()})

    unreal.log("LANDMARK_INVENTORY " + json.dumps(report))
    print("BLUEPRINTS", len(report["blueprints"]))
    print("STATIC_MESHES", len(report["static_meshes"]))
    print("OTHER", len(report["other"]))


def blueprint_parent(blueprint):
    try:
        parent = blueprint.get_editor_property("parent_class")
        return parent.get_name() if parent else None
    except Exception:
        return None


if __name__ == "__main__":
    main()
