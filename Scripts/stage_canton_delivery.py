"""Stage the scoped Canton cook and repair UE 5.5's Mac CEF runtime lookup."""
import subprocess,json,os,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'QA/Canton_Continuation'
assert json.loads((OUT/'Delivery_Cook.json').read_text())['passed']
cmd=['/Volumes/M2/Engine/UE_5.5/Engine/Build/BatchFiles/RunUAT.sh','BuildCookRun','-project='+str(ROOT/'Aura.uproject'),'-platform=Mac','-clientconfig=Development','-skipbuild','-skipcook','-stage','-pak','-stagingdirectory='+str(ROOT/'Saved/CantonContinuation/Stage'),'-unattended','-nop4']
with (ROOT/'Saved/CantonContinuation/Stage.log').open('w') as f:r=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT,cwd=ROOT)
assert r.returncode==0,'Stage failed'
app=ROOT/'Saved/CantonContinuation/Stage/Mac/Aura.app'
# Xcode embeds CEF under Frameworks, but UE 5.5's WebBrowser still resolves
# Engine/Binaries/ThirdParty/CEF3/Mac. Keep one copy with a relocatable link.
cef=app/'Contents/Frameworks/Chromium Embedded Framework.framework'
assert (cef/'Resources/icudtl.dat').is_file()
link=app/'Contents/UE/Engine/Binaries/ThirdParty/CEF3/Mac'/cef.name
link.parent.mkdir(parents=True,exist_ok=True)
if not link.exists():link.symlink_to(os.path.relpath(cef,link.parent))
assert (link/'Resources/icudtl.dat').is_file()
subprocess.run(['/usr/bin/codesign','--force','--sign','-','--preserve-metadata=identifier,entitlements,flags',str(app)],check=True)
binary=app/'Contents/MacOS/Aura'
(OUT/'Delivery_Stage.json').write_text(json.dumps({'passed':True,'command':cmd,'app':str(app.relative_to(ROOT)),'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'cef_runtime_link':str(link.relative_to(app)),'link_target':os.readlink(link),'normal_project_packaging_claim':False},indent=2)+'\n')
print(app)
