"""Validate frozen technical M01 evidence and report each historical gate."""
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
    budget=(ROOT/'Docs/Terrain_Acceptance_Budget.md').read_text()
    if '| `owner_approval` | `pending` |' not in budget:
        raise ValueError('Day 01 owner approval state changed; review required')
    with (ROOT/'QA/Map_GCP_Residuals.csv').open(newline='') as stream:
        gcps=list(csv.DictReader(stream))
    fit=[row for row in gcps if row['role_control_or_holdout']=='control']
    holdouts=[row for row in gcps if row['role_control_or_holdout']=='holdout']
    if len(fit)!=5 or len(holdouts)!=3 or any(row['status']!='candidate_not_surveyed' for row in gcps):
        raise ValueError('H1 evidence changed; review required')
    with (ROOT/'Data/Gate_District_Candidates.csv').open(newline='') as stream:
        districts=list(csv.DictReader(stream))
    named=[row for row in districts if row['bounds_status']=='NO_METRIC_BOUNDS']
    legacy=[row for row in districts if row['bounds_status']=='PROVISIONAL_DIAGNOSTIC_ONLY']
    if ({row['candidate_id'] for row in named}!={'GATE-WENMINGMEN','GATE-ZHENGDONGMEN'}
            or len(legacy)!=2 or any(row['h2_status']!='BLOCKED' or row['h2_control_source'] for row in districts)):
        raise ValueError('H2 candidate status changed; review required')
    with (ROOT/'Data/Elevation_Constraints.csv').open(newline='') as stream:
        elevations=list(csv.DictReader(stream))
    if not elevations or any(row['height_m'] or row['status']!='candidate_stratum_not_terrain' for row in elevations):
        raise ValueError('Historical Z evidence changed; review required')
    print(json.dumps({
        'technical_ue':'Pass — provisional modern-context scope',
        'historical_xy_city_h1':'Blocked',
        'historical_xy_gate_local_h2':'Blocked',
        'historical_z':'Blocked',
        'owner_approval':'pending',
        'historical_m01':'Blocked',
        'native_artifacts':len(rows),
        'automation_tests_passed':1,
        'h1_fit_candidates_unsurveyed':len(fit),
        'h1_independent_candidates_unsurveyed':len(holdouts),
        'h2_accepted_local_checks':0,
        'historical_z_accepted_points':0,
        'survey_evidence_required':'QA/Control_Survey_Handoff.md'
    },indent=2))

if __name__=='__main__':main()
