"""Freeze the provisional native map, external actors and supporting evidence."""
import csv
import hashlib
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]

def artifact_paths():
    files=[
        '.gitignore','Source/AuraEditor/AuraEditor.Build.cs',
        'Source/AuraEditor/Public/Terrain/CantonTerrainLibrary.h',
        'Source/AuraEditor/Private/Terrain/CantonTerrainLibrary.cpp',
        'Source/AuraEditor/Private/Tests/CantonTerrainTests.cpp',
        'Scripts/prepare_canton_ue_import.py','Scripts/ImportCantonProvisionalTerrain.py',
        'Scripts/ReviewCantonProvisionalTerrain.py','Scripts/run_canton_provisional_review.sh',
        'Scripts/test_canton_ue_import_contract.py','Scripts/refresh_canton_native_manifest.py',
        'Scripts/test_canton_m01_readiness.py','Data/Canton_Prototype_Contract.json',
        'Data/Artifact_Manifest.csv','Data/M01_Provisional_Artifacts.csv',
        'Export/Provisional/Canton_Modern_Context_SouthFirst.r16',
        'Export/Provisional/UE_Overlay_Metres.json',
        'QA/Canton_Days_01_10_Execution.md','QA/Canton_Blocker_Research_2026_09_26.md',
        'Docs/Reports/Change-Archive/2026-09-26-canton-native-terrain-validation.svg',
        'Docs/Reports/Change-Archive/2026-09-26-canton-native-terrain-validation.md',
    ]
    for folder in ('Content/Canton/Provisional','Content/__ExternalActors__/Canton/Provisional'):
        files.extend(str(p.relative_to(ROOT)) for p in (ROOT/folder).rglob('*') if p.suffix in ('.uasset','.umap'))
    for name in ('Creation_Validation.json','Reload_Validation.json','Automation_Validation.json','Automation_Report.json','Import_Settings.json','Capture_Settings.json','overview.png','north-hills.png','south-gate.png'):
        files.append('QA/UE_Import_Screenshots/'+name)
    return sorted(files)


def main():
    files=artifact_paths()
    with (ROOT/'Data/M01_Native_Artifacts.csv').open('w',newline='') as stream:
        w=csv.writer(stream,lineterminator='\n')
        w.writerow(['path','sha256','bytes','status','owner','source_inputs','license','tool_version'])
        for relative in files:
            data=(ROOT/relative).read_bytes()
            w.writerow([relative,hashlib.sha256(data).hexdigest(),len(data),'Provisional technical; historical acceptance excluded','AuraProj terrain','ELEV-002; MAP-001; OSM-001; prototype contract; repository tooling','Project-authored; DTM CC BY 4.0; map public domain; OSM-derived overlay ODbL','UE 5.5.4 CL 40574608; pinned terrain Python environment'])
    print(f'Frozen {len(files)} native artifacts, including external actor packages')

if __name__=='__main__':main()
