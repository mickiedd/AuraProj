#!/usr/bin/env python3
import importlib.util, json, shutil, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('validator',ROOT/'Scripts/validate_gameplay_expansion_content.py');v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
manifest=ROOT/'Content/Config/GameplayExpansionManifest.json'
r=v.validate(ROOT,Path('Content/Config/GameplayExpansionManifest.json'));assert r['validatedFiles']>=10
with tempfile.TemporaryDirectory() as temp:
    stage=Path(temp)
    for row in json.loads(manifest.read_text())['files']:
        target=stage/row['path'];target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/row['path'],target)
    assert v.validate(ROOT,Path('Content/Config/GameplayExpansionManifest.json'),stage)['stagedCompared']
    first=json.loads(manifest.read_text())['files'][0]['path'];(stage/first).write_bytes(b'tampered')
    try:v.validate(ROOT,Path('Content/Config/GameplayExpansionManifest.json'),stage);raise AssertionError('tamper accepted')
    except v.ContentError:pass
print('Day57 content contracts: PASS (3/3)')
