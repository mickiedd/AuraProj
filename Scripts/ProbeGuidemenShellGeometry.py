"""Measure Guidemen's roof shells from their real vertex geometry.

An instance AABB cannot express a slope, and the tile normals say the tiled surface
is correct while the shells reach 148 cm above the ridge. This reads each shell
mesh's vertex positions through `StaticMesh.get_static_mesh_description`, transforms
them by the instance, and reports the world Z at the shell's inner (ridge-side) and
outer (eave-side) edges along whichever horizontal axis the shell actually slopes in.

Also reports the same for the tiles' aggregate surface so the shell and tile
surfaces can be compared directly.

Read-only.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
OUTPUT = PROJECT / "Saved/RawModelImport/Guidemen_V5_4K-shell-geometry-20260918.json"
BLUEPRINT = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/BP_Guidemen_V5_4K"


def _path(value):
    return value.get_path_name() if value else ""


def _components(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    seen, result = set(), []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        component = library.get_object(data)
        if not isinstance(component, unreal.HierarchicalInstancedStaticMeshComponent):
            continue
        if _path(component) in seen:
            continue
        seen.add(_path(component))
        result.append(component)
    return result


def _positions(mesh):
    try:
        description = mesh.get_static_mesh_description(0)
    except Exception as error:
        return None, "get_static_mesh_description failed: " + repr(error)[:100]
    if description is None:
        return None, "no description"
    api = [n for n in dir(description) if "position" in n.lower() or "vertex" in n.lower()]
    for name in ("get_vertex_positions", "get_vertex_instance_positions"):
        if not hasattr(description, name):
            continue
        try:
            values = getattr(description, name)()
            return values, "ok via " + name + " | api " + json.dumps(api)
        except Exception as error:
            last = name + ": " + repr(error)[:100]
    return None, "no usable accessor | api " + json.dumps(api) + " | " + repr(locals().get("last", ""))


def main():
    blueprint = unreal.EditorAssetLibrary.load_asset(BLUEPRINT)
    assert isinstance(blueprint, unreal.Blueprint), BLUEPRINT
    report = {"blueprint": BLUEPRINT, "shells": [], "notes": []}
    for component in _components(blueprint):
        mesh = component.static_mesh
        if not mesh:
            continue
        text = (component.get_name() + " " + _path(mesh)).lower()
        if "roofshell" not in text and "t0" not in text and "tile" not in text:
            continue
        positions, note = _positions(mesh)
        report["notes"].append({"component": component.get_name(), "note": note})
        if positions is None:
            print("SHELL_NO_VERTICES", component.get_name(), note)
            continue
        local = [(float(p.x), float(p.y), float(p.z)) for p in positions]
        # sample the instances: all for shells, a subset for the 16k tile component
        count = component.get_instance_count()
        step = max(1, count // 400)
        rows = []
        for index in range(0, count, step):
            transform = component.get_instance_transform(index, False)
            quat = transform.rotation
            scale = transform.scale3d
            world = []
            for x, y, z in local:
                rotated = unreal.MathLibrary.quat_rotate_vector(
                    quat, unreal.Vector(x * scale.x, y * scale.y, z * scale.z))
                point = rotated + transform.translation
                world.append((float(point.x), float(point.y), float(point.z)))
            rows.append({"index": index, "world": world})
        entry = {
            "component": component.get_name(),
            "mesh": _path(mesh).split("/")[-1].split(".")[0],
            "instance_count": count,
            "sampled_instances": len(rows),
            "local_vertex_count": len(local),
            "local_extents": [round(max(v[a] for v in local) - min(v[a] for v in local), 2) for a in range(3)],
        }
        # aggregate the world vertices and look at how Z varies with X and Y
        flat = [v for row in rows for v in row["world"]]
        def corr(pairs):
            if len(pairs) < 8:
                return None
            xs = [p[0] for p in pairs]
            ys = [p[1] for p in pairs]
            mx, my = sum(xs) / len(xs), sum(ys) / len(ys)
            num = sum((x - mx) * (y - my) for x, y in pairs)
            den = (sum((x - mx) ** 2 for x in xs) * sum((y - my) ** 2 for y in ys)) ** 0.5
            return round(num / den, 4) if den else None
        entry["corr_worldZ_vs_worldX"] = corr([(v[0], v[2]) for v in flat])
        entry["corr_worldZ_vs_worldY"] = corr([(v[1], v[2]) for v in flat])
        entry["world_z_range"] = [round(min(v[2] for v in flat), 2), round(max(v[2] for v in flat), 2)]
        entry["world_x_range"] = [round(min(v[0] for v in flat), 2), round(max(v[0] for v in flat), 2)]
        entry["world_y_range"] = [round(min(v[1] for v in flat), 2), round(max(v[1] for v in flat), 2)]
        # Z at the inner and outer edge along the dominant slope axis
        axis = 0 if abs(entry["corr_worldZ_vs_worldX"] or 0) > abs(entry["corr_worldZ_vs_worldY"] or 0) else 1
        values = sorted({round(v[axis], 1) for v in flat})
        if len(values) >= 4:
            near, far = values[0], values[-1]
            band = max((far - near) * 0.15, 5.0)
            inner = [v[2] for v in flat if abs(v[axis] - near) <= band]
            outer = [v[2] for v in flat if abs(v[axis] - far) <= band]
            entry["slope_axis"] = ["X", "Y"][axis]
            entry["axis_min_max_cm"] = [near, far]
            entry["mean_z_at_axis_min"] = round(sum(inner) / len(inner), 2) if inner else None
            entry["mean_z_at_axis_max"] = round(sum(outer) / len(outer), 2) if outer else None
            if entry["mean_z_at_axis_min"] is not None and entry["mean_z_at_axis_max"] is not None:
                entry["z_delta_along_axis_cm"] = round(entry["mean_z_at_axis_max"] - entry["mean_z_at_axis_min"], 2)
        report["shells"].append(entry)
        print("SHELL", entry["component"], entry["mesh"], "n", count,
              "corrZ_X", entry["corr_worldZ_vs_worldX"], "corrZ_Y", entry["corr_worldZ_vs_worldY"],
              "axis", entry.get("slope_axis"), "axisRange", entry.get("axis_min_max_cm"),
              "zAtMin", entry.get("mean_z_at_axis_min"), "zAtMax", entry.get("mean_z_at_axis_max"),
              "delta", entry.get("z_delta_along_axis_cm"), "zRange", entry["world_z_range"])
    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("SHELL_OUTPUT", OUTPUT)


if __name__ == "__main__":
    main()
