"""Cook the two-map Canton delivery profile without editing DefaultGame.ini.
This profile excludes unrelated always-cook folders; normal project cook is a
separate gate and must not be inferred from this scoped artifact.
"""
import hashlib,json,subprocess,re,os
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
editor='/Volumes/M2/Engine/UE_5.5/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor'
config=ROOT/'Config/DefaultGame.ini';before=hashlib.sha256(config.read_bytes()).hexdigest()
section='[/Script/UnrealEd.ProjectPackagingSettings]'
excluded=['/NNEDenoiser','/Game/Assets','/Game/Blueprints','/Game/MilitaryWeapDark']
override='-ini:Game:'+','.join(section+':-DirectoriesToAlwaysCook=(Path="'+p+'")' for p in excluded)
maps=['/Game/Canton/Provisional/Maps/L_Canton_WalledCity_PROVISIONAL','/Game/Canton/DistrictPrototype/Maps/L_Canton_District_PROVISIONAL']
log=ROOT/'Saved/Logs/CantonDeliveryCook.log'
cmd=[editor,str(ROOT/'Aura.uproject'),'-run=cook','-targetplatform=Mac','-map='+'+'.join(maps),override,'-unattended','-nop4','-nosplash','-NoSound','-abslog='+str(log)]
with (ROOT/'Saved/CantonContinuation/DeliveryCook.stdout').open('w') as f:r=subprocess.run(cmd,cwd=ROOT,env={**os.environ,"LC_ALL":"C","PYTHONCOERCECLOCALE":"0"},stdout=f,stderr=subprocess.STDOUT)
text=log.read_text(errors='replace');errors=[line for line in text.splitlines() if 'Error:' in line or 'Fatal error' in line or 'Failed to compile Material' in line or 'Default Material will be used' in line]
counts=re.findall(r'Cooked packages (\d+) Packages Remain (\d+) Total (\d+)',text)
assert counts, 'Cook completion counters missing'
cooked,remaining,total=map(int,counts[-1])
report={'packages_processed':cooked,'packages_remaining':remaining,'packages_total':total,'profile':'Canton two-map isolated delivery','command':cmd,'exit_code':r.returncode,'excluded_unrelated_always_cook':excluded,'source_config_sha256':before,'source_config_unchanged':hashlib.sha256(config.read_bytes()).hexdigest()==before,'normal_project_cook_claim':False,'errors':errors,'log':str(log.relative_to(ROOT)),'passed':r.returncode==0 and not errors and remaining==0 and hashlib.sha256(config.read_bytes()).hexdigest()==before}
(ROOT/'QA/Canton_Continuation/Delivery_Cook.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2));raise SystemExit(not report['passed'])
