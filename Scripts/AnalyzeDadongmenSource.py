import json
import pathlib
import struct

root = pathlib.Path(__file__).resolve().parents[1] / "Saved/RawModelImport/V4/Dadongmen_GreatEastGate_UE5/Meshes"
doc = json.loads((root / "SM_Dadongmen_LOD0.gltf").read_text())
binary = (root / doc["buffers"][0]["uri"]).read_bytes()
accessor = doc["accessors"][0]
view = doc["bufferViews"][accessor["bufferView"]]
stride = view.get("byteStride", 12)
positions = [struct.unpack_from("<3f", binary, view.get("byteOffset", 0) + i * stride) for i in range(accessor["count"])]
print("COUNT", len(positions), "BOUNDS", accessor["min"], accessor["max"])
for y in (-11.15, -10.8, -10.0, 9.5, 10.0):
    near = [p for p in positions if abs(p[1] - y) < .18]
    print("Y", y, "COUNT", len(near), "XRANGE", (min(p[0] for p in near), max(p[0] for p in near)) if near else None,
          "ZRANGE", (min(p[2] for p in near), max(p[2] for p in near)) if near else None)
for ylo, yhi in [(-11.2, -10.5), (-10.5, -9.5), (9.0, 10.3)]:
    print("SLICE", ylo, yhi)
    for zi in range(0, 10):
        near = [p for p in positions if ylo <= p[1] <= yhi and zi <= p[2] < zi + 1]
        if near:
            xs = sorted({round(p[0], 2) for p in near})
            print(zi, len(near), min(xs), max(xs), xs[:6], xs[-6:])
for zlo, zhi in [(0, .2), (.8, 1.0), (1.5, 1.7), (2.5, 2.7), (3.5, 3.7), (4.5, 4.7)]:
    near = [p for p in positions if zlo <= p[2] <= zhi]
    ys = sorted({round(p[1], 2) for p in near})
    print("Z", zlo, zhi, "COUNT", len(near), "YRANGE", (min(ys), max(ys)) if ys else None, "YRANGE_HEAD", ys[:8], "TAIL", ys[-8:])
