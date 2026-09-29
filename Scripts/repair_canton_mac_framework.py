"""Normalize the generated game's UE-supplied CEF framework for current Xcode.
Only touches Binaries/Mac/Aura.app, never the installed engine or shared source.
Run after UBT links; then rerun the generated Xcode post-build target.
"""
from pathlib import Path
import plistlib
root=Path(__file__).resolve().parents[1]
f=root/'Binaries/Mac/Aura.app/Contents/Frameworks/Chromium Embedded Framework.framework'
plist=f/'Resources/Info.plist'
assert plist.is_file(),plist
assert plistlib.loads(plist.read_bytes())['CFBundlePackageType']=='FMWK'
a=f/'Versions/A';a.mkdir(parents=True,exist_ok=True)
for name in ('Chromium Embedded Framework','Resources','Libraries','_CodeSignature'):
 p=f/name
 if p.exists() and not p.is_symlink():
  assert not (a/name).exists(),name
  p.rename(a/name)
 if name!='_CodeSignature' and not p.exists():p.symlink_to('Versions/Current/'+name)
current=f/'Versions/Current'
if not current.exists():current.symlink_to('A')
# The temporary shallow-bundle link is invalid on macOS; use standard versioned resources.
if (f/'Info.plist').is_symlink():(f/'Info.plist').unlink()
assert (f/'Versions/Current/Resources/Info.plist').is_file()
print('Generated CEF framework now uses Versions/Current; rerun Xcode signing/validation')
