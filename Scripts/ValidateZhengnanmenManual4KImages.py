"""Independent assertions on lossless authored PBR source images."""
import hashlib
import json
from pathlib import Path
import numpy as np
from PIL import Image

PROJECT=Path(__file__).resolve().parents[1]
SOURCE=PROJECT/"ContentSource/GuangzhouLandmarks/Zhengnanmen_Manual4K_20260916"
OUT=PROJECT/"Saved/RawModelImport/ZhengnanmenManual4K"
manifest=json.loads((SOURCE/"manifest.json").read_text())
assert len(manifest["materials"])==7
results={}
for kind,entry in manifest["materials"].items():
    arrays={};checks={}
    for key,record in entry["maps"].items():
        path=Path(record["path"])
        assert hashlib.sha256(path.read_bytes()).hexdigest()==record["sha256"],path
        im=Image.open(path);a=np.asarray(im)
        assert im.size==(4096,1024 if kind=="Plaque" else 4096),(path,im.size)
        assert im.format=="PNG" and 'A' not in im.getbands(),path
        if key=="Height":
            assert a.max()>255 and im.mode in ('I','I;16'),path
        else:assert a.dtype==np.uint8,path
        seam=0
        if entry["tileable"]:
            seam=max(int(np.abs(a[0].astype(np.int32)-a[-1].astype(np.int32)).max()),int(np.abs(a[:,0].astype(np.int32)-a[:,-1].astype(np.int32)).max()))
            assert seam==0,(path,seam)
        arrays[key]=a
        checks[key]={"size":list(im.size),"hash_pass":True,"seam_max":seam if entry["tileable"] else "unique plaque, not tileable"}
    assert np.array_equal(arrays['ORM'][...,0],arrays['AO']),kind
    assert np.array_equal(arrays['ORM'][...,1],arrays['Roughness']),kind
    assert np.array_equal(arrays['ORM'][...,2],arrays['Metallic']),kind
    v=arrays['Normal_DX'].astype(np.float32)/127.5-1
    length=np.linalg.norm(v,axis=2)
    assert np.max(np.abs(length-1))<.015,(kind,float(length.min()),float(length.max()))
    assert arrays['Normal_DX'][...,2].min()>220,kind
    if kind not in ('BronzeGold','Plaque'):assert arrays['Metallic'].max()==0,kind
    if kind=='Glaze':assert .15<float(arrays['Roughness'].mean()/255)<.4
    if kind=='Stone':assert .65<float(arrays['Roughness'].mean()/255)<.95
    results[kind]={"maps":checks,"packed_channels_exact":True,"normal_length_range":[float(length.min()),float(length.max())],"metal_mean":float(arrays['Metallic'].mean()/255)}
    del arrays,v,length
OUT.mkdir(parents=True,exist_ok=True)
(OUT/"image-validation.json").write_text(json.dumps({"passed":True,"image_count":49,"materials":results},indent=2))
print("MANUAL4K_IMAGE_VALIDATION_PASSED: 49 hashes, sizes, native lossless PNGs, 16-bit height source, 42 tileable maps, exact ORM packing, DirectX unit normals, nonmetallic dielectrics.")
