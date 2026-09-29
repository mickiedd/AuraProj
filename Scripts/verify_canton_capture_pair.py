"""Check a fixed-camera dry/wet pair; changed area is a smoke check, not art approval."""
import json,hashlib
from pathlib import Path
import numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'QA/Canton_District'
settings=json.loads((OUT/'Capture_Settings.json').read_text())
a,b=[next(v for v in settings if v['view']==name) for name in ('dry_ground','after_rain_ground')]
x,y=[np.asarray(Image.open(ROOT/v['path']).convert('RGB'),dtype=np.int16) for v in (a,b)]
assert x.shape==y.shape
mask=np.max(abs(y-x),axis=2)>8
changed=int(mask.sum());fraction=float(mask.mean())
record={'matched_camera':a['camera_cm']==b['camera_cm'],'matched_target':a['target_cm']==b['target_cm'],'image_size':[x.shape[1],x.shape[0]],'changed_pixels_gt8':changed,'changed_fraction_gt8':fraction,'dry_visible':not a['damp_actors_hidden'],'damp_visible':not b['damp_actors_hidden'],'mean_changed_rgb_delta':float((y-x)[mask].mean()) if changed else None,'dry_sha256':hashlib.sha256((ROOT/a['path']).read_bytes()).hexdigest(),'wet_sha256':hashlib.sha256((ROOT/b['path']).read_bytes()).hexdigest(),'scope':'paired-capture smoke check; period-art acceptance requires separate review','threshold_rationale':'A nonzero localized change is required. The former >20000 pixel assertion encoded the rectangular blockout footprint; the new irregular mesh has a different silhouette. Require >1000 visible changed pixels and <10% of frame, plus darker mean color.','visual_decision':'Review the current images; no automated historical art approval.'}
record['passed']=record['matched_camera'] and record['matched_target'] and changed>1000 and fraction<.1 and record['mean_changed_rgb_delta']<0
(OUT/'Capture_QA.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record,indent=2));raise SystemExit(not record['passed'])
