"""Consolidate the supplied Xiaobeimen v4 OBJ groups by material.

The v4 package is geometry-only repaired, so its existing texture/material set
is retained.  This derived OBJ keeps all vertices and UVs and only merges the
many source objects into one importable mesh with one group per material.
"""
from collections import defaultdict
from pathlib import Path
import json

ROOT = Path(
    "C:/Works/Raw3DModels/Extracted/"
    "Xiaobeimen_SmallNorthGate_Unreal_v4_GeometryFixed/"
    "Xiaobeimen_SmallNorthGate_Unreal_v4_GeometryFixed"
)
SOURCE = ROOT / "SM_Xiaobeimen_SmallNorthGate_Textured_GeometryFixed.obj"
OUTPUT = ROOT / "SM_Xiaobeimen_Combined_GeometryFixed.obj"

attributes = []
faces = defaultdict(list)
material = None
counts = {"v": 0, "vt": 0, "vn": 0}

for line in SOURCE.read_text(encoding="utf-8", errors="strict").splitlines(True):
    kind = line.split(" ", 1)[0]
    if kind in counts:
        attributes.append(line)
        counts[kind] += 1
    elif kind == "usemtl":
        material = line.split()[1]
    elif kind == "f":
        assert material, "face encountered before usemtl"
        refs = []
        for vertex in line.split()[1:]:
            parts = vertex.split("/")
            normalized = []
            for index, value in enumerate(parts):
                if not value:
                    normalized.append("")
                    continue
                number = int(value)
                total = (counts["v"], counts["vt"], counts["vn"])[index]
                normalized.append(str(number if number > 0 else total + number + 1))
            refs.append("/".join(normalized))
        faces[material].append("f " + " ".join(refs) + "\n")

with OUTPUT.open("w", encoding="utf-8", newline="\n") as stream:
    stream.write("# Derived from supplied Xiaobeimen v4 GeometryFixed OBJ; cm, Z-up.\n")
    stream.writelines(attributes)
    for material_name, material_faces in faces.items():
        stream.write("o " + material_name + "\n")
        stream.write("usemtl " + material_name + "\n")
        stream.writelines(material_faces)

report = {
    "source": str(SOURCE),
    "output": str(OUTPUT),
    "attribute_counts": counts,
    "material_groups": len(faces),
    "material_face_counts": {name: len(items) for name, items in faces.items()},
    "faces": sum(len(items) for items in faces.values()),
}
print(json.dumps(report, indent=2))
