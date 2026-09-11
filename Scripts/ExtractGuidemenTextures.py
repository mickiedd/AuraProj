"""Extract embedded Guidemen GLB PNGs for deterministic UE material repair."""
import hashlib
import json
import struct
from pathlib import Path


REQUESTED_SOURCE = Path("C:/Works/Raw3DModels/Guidemen/_HighPoly/_UE5/_4K.glb")
FLAT_SOURCE = Path("C:/Works/Raw3DModels/Guidemen_HighPoly_UE5_4K.glb")
SOURCE = next((path for path in (REQUESTED_SOURCE, FLAT_SOURCE) if path.is_file()), None)
assert SOURCE is not None, (REQUESTED_SOURCE, FLAT_SOURCE)

PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = PROJECT_ROOT / "Saved" / "RawModelImport" / "GuidemenTextures"
REPORT_PATH = PROJECT_ROOT / "Saved" / "RawModelImport" / "Guidemen-source.json"

raw = SOURCE.read_bytes()
assert raw[:4] == b"glTF", "Not a GLB file"
json_length, json_type = struct.unpack_from("<I4s", raw, 12)
assert json_type == b"JSON"
gltf = json.loads(raw[20 : 20 + json_length].decode("utf-8").rstrip(" "))
binary_offset = 20 + json_length
binary_length, binary_type = struct.unpack_from("<I4s", raw, binary_offset)
assert binary_type == b"BIN\x00"
binary = raw[binary_offset + 8 : binary_offset + 8 + binary_length]

OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
images = []
for image in gltf.get("images", []):
    buffer_view = gltf["bufferViews"][image["bufferView"]]
    start = buffer_view.get("byteOffset", 0)
    payload = binary[start : start + buffer_view["byteLength"]]
    filename = image["name"]
    target = OUTPUT_DIR / filename
    if target.exists():
        assert target.read_bytes() == payload, "Refusing to overwrite changed extraction: " + str(target)
    else:
        target.write_bytes(payload)
    images.append({"name": filename, "path": str(target), "sha256": hashlib.sha256(payload).hexdigest(), "bytes": len(payload)})

REPORT_PATH.write_text(
    json.dumps({"source": str(SOURCE), "images": images}, indent=2), encoding="utf-8"
)
print("GUIDEMEN_TEXTURES_EXTRACTED", json.dumps({"source": str(SOURCE), "count": len(images), "output": str(OUTPUT_DIR)}))
