"""Validate frozen technical M01 evidence without granting historical acceptance."""
import csv
import hashlib
import json
from pathlib import Path
from refresh_canton_native_manifest import artifact_paths

ROOT=Path(__file__).resolve().parents[1]

def main():
    rows=list(csv.DictReader((ROOT/'Data/M01_Native_Artifacts.csv').open()))
    if sorted(r['path'] for r in rows)!=artifact_paths():raise ValueError('Native manifest coverage mismatch')
    for row in rows:
        data=(ROOT/row['path']).read_bytes()
        if hashlib.sha256(data).hexdigest()!=row['sha256'] or len(data)!=int(row['bytes']):
            raise ValueError('Native artifact changed: '+row['path'])
    contract=json.loads((ROOT/'Data/Canton_Prototype_Contract.json').read_text())
    if contract['historically_accepted'] is not False:raise ValueError('Unsupported historical promotion')
    for name in ('Creation_Validation','Reload_Validation','Automation_Validation'):
        result=json.loads((ROOT/f'QA/UE_Import_Screenshots/{name}.json').read_text())
        if not result['passed'] or result['errors'] or result['historically_accepted']:raise ValueError(name)
        if result['landscape_components']!=256 or result['collision_components']!=256:raise ValueError(name)
        if result['height_and_collision_checks']!=7 or result['max_collision_error_cm']>=2:raise ValueError(name)
    report=json.loads((ROOT/'QA/UE_Import_Screenshots/Automation_Report.json').read_text(encoding='utf-8-sig'))
    if (report['succeeded'],report['failed'],report['succeededWithWarnings'])!=(1,0,0):raise ValueError('Automation failed')
    if report['tests'][0]['fullTestPath']!='Aura.Canton.Provisional.MapConfiguration':raise ValueError('Wrong automation test')
    expected_map=ROOT/'Content/Canton/Provisional/Maps/L_Canton_WalledCity_PROVISIONAL.umap'
    if not expected_map.is_file():raise ValueError('Map missing')
    print(json.dumps({'technical_m01':'Provisional validated','historical_m01':'Blocked','native_artifacts':len(rows),'automation_tests_passed':1,'survey_evidence_required':'QA/Control_Survey_Handoff.md'},indent=2))

if __name__=='__main__':main()
