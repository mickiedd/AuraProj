"""Normalize malformed inline ASCII FBX records without changing geometry arrays."""
from pathlib import Path
import re,json,hashlib
ROOT=Path(__file__).resolve().parents[1]/'Saved/RawModelImport/V4'
def main():
 records=[]
 for src in (ROOT/'Zhengximen_GreatWestGate_UE5/Meshes').glob('*.fbx'):
  text=src.read_text(); before=re.findall(r'a:\s*([^}]+)',text)
  counts={kind:len(re.findall(r'^\s*'+kind+r':',text,re.M)) for kind in ('Model','Geometry','Material')}
  definitions='Definitions: {\nVersion: 100\nCount: '+str(sum(counts.values()))+'\n'+''.join(f'ObjectType: "{k}" {{ Count: {v} }}\n' for k,v in counts.items())+'}\n'
  text=re.sub(r'Definitions:\s*\{[^}]*\}',definitions,text,count=1)
  # FBX ASCII properties must start on distinct lines; quoted names stay literal.
  text=re.sub(r'"[^"\n]*"|[A-Za-z_][A-Za-z_0-9]*:|[{}]',lambda m:m[0] if m[0].startswith('"') else '\n'+m[0]+'\n' if m[0] in '{}' else '\n'+m[0],text)
  after=re.findall(r'a:\s*([^}]+)',text)
  assert [re.sub(r'\s','',x) for x in before]==[re.sub(r'\s','',x) for x in after]
  dst=ROOT/'Prepared/Zhengximen'/src.name;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_text(text)
  records.append({'source':str(src),'prepared':str(dst),'geometry_arrays_unchanged':True,'object_counts':counts,'sha256':hashlib.sha256(dst.read_bytes()).hexdigest()})
 cfgs=json.loads((ROOT/'packages.json').read_text())
 for cfg in cfgs:
  if cfg['name']=='Zhengximen_V4':
   for mesh in cfg['meshes']:mesh['source']=str(ROOT/'Prepared/Zhengximen'/Path(mesh['source']).name)
 (ROOT/'packages.json').write_text(json.dumps(cfgs,indent=2));(ROOT/'fbx-repair.json').write_text(json.dumps(records,indent=2))
 print('Prepared',len(records),'FBX files; numerical arrays unchanged')
if __name__=='__main__':main()
