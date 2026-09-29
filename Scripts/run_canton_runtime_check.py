"""Fail-closed launcher for actual game (not editor SceneCapture) evidence."""
import argparse,json,subprocess,time,plistlib,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def validate(data,mode):
 errors=[]
 if data.get('editor_binary') is not False:errors.append('Requires a standalone game binary')
 if data.get('pawn_traversal_performed') is not True:errors.append('No physical pawn evidence')
 if data.get('historically_accepted') is not False:errors.append('Historical promotion forbidden')
 if data.get('passed') is not True or data.get('errors'):errors.append('Native runtime gate failed')
 if mode=='traversal':
  rows=data.get('routes',[])
  if {r.get('route') for r in rows}!={'principal','mixed_west','mixed_east','intersection','threshold','gutter','courtyard_reference'}:errors.append('Missing route coverage')
  if any(not r.get('passed') or r.get('fell') or r.get('endpoint_error_cm',1e9)>=50 or r.get('walked_cm',0)<=0 for r in rows):errors.append('Invalid physical route')
 else:
  if data.get('world_partition_streaming_enabled') is not True or data.get('streaming_level_count',0)<=0:errors.append('World Partition streaming not exercised')
  if (data.get('warmup_seconds'),data.get('capture_seconds'))!=(60,180):errors.append('Wrong sampling window')
  if (data.get('width'),data.get('height'))!=(1920,1080):errors.append('Wrong resolution')
  settings=data.get('render_settings',{})
  for group in ('ViewDistance','AntiAliasing','Shadow','GlobalIllumination','Reflection','PostProcess','Texture','Effects','Foliage','Shading'):
   if settings.get('sg.'+group+'Quality')!=2:errors.append('Not High: '+group)
  if settings.get('r.ScreenPercentage')!=100:errors.append('Internal rendering resolution reduced')
  if settings.get('r.DynamicGlobalIlluminationMethod')!=1 or settings.get('r.ReflectionMethod')!=1:errors.append('Lumen not configured')
  if data.get('frame_samples',0)<1000 or data.get('gpu_samples',0)<1000:errors.append('Incomplete frame/GPU sampling')
  for key,limit in [('p95_ms',33.3),('p99_ms',50),('rss_peak_bytes',12*1024**3)]:
   if not 0<data.get(key,1e99)<=limit:errors.append('Budget: '+key)
  if data.get('failed_cell_samples')!=0 or data.get('fell') or data.get('walked_cm',0)<19500:errors.append('Streaming or route failed')
  if data.get('draw_calls_max',0)<=0 or data.get('loaded_mesh_components',0)<=0:errors.append('Missing render counters')
 return errors

def main():
 p=argparse.ArgumentParser();p.add_argument('mode',choices=['traversal','performance']);p.add_argument('--binary',type=Path,required=True);p.add_argument('--extra',action='append',default=[]);a=p.parse_args()
 out=ROOT/'QA/Canton_Continuation'/('Runtime_'+a.mode+'.json');out.parent.mkdir(exist_ok=True);out.unlink(missing_ok=True)
 for suffix in ('.csv','.png'):out.with_suffix(suffix).unlink(missing_ok=True)
 log=ROOT/'Saved/Logs'/('CantonRuntime_'+a.mode+'.log')
 app=next((p for p in a.binary.resolve().parents if p.suffix=='.app'),None)
 assert app, 'A packaged Mac app is required'
 bundle=plistlib.loads((app/'Contents/Info.plist').read_bytes())['CFBundleIdentifier']
 # Preserve the macOS app sandbox; collect results from its writable container.
 container=Path.home()/'Library/Containers'/bundle/'Data/Documents/CantonQA'
 container.mkdir(parents=True,exist_ok=True)
 runtime_out=container/out.name;runtime_out.unlink(missing_ok=True)
 for suffix in ('.csv','.png'):out.with_suffix(suffix).unlink(missing_ok=True)
 for suffix in ('.csv','.png'):runtime_out.with_suffix(suffix).unlink(missing_ok=True)
 runtime_log=container/log.name
 command=[str(a.binary.resolve()),'/Game/Canton/DistrictPrototype/Maps/L_Canton_District_PROVISIONAL','-CantonProbe='+a.mode,'-CantonOutput='+str(runtime_out),'-windowed','-ResX=1920','-ResY=1080','-ForceRes','-NoVSync','-unattended','-nosplash','-NoSound','-abslog='+str(runtime_log),'-nocef','-stdout','-FullStdOutLogOutput',*a.extra]
 start=time.time()
 with log.open('w') as stream:
  process=subprocess.Popen(command,cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT)
  # Activate this PID only; 'open -a' can race startup and launch another copy.
  time.sleep(2)
  activation="ObjC.import('AppKit'); var app=$.NSRunningApplication.runningApplicationWithProcessIdentifier(%d); app.activateWithOptions(2);" % process.pid
  subprocess.run(['osascript','-l','JavaScript','-e',activation],check=True,stdout=subprocess.DEVNULL)
  try:code=process.wait(timeout=600)
  except subprocess.TimeoutExpired:
   process.terminate();process.wait(timeout=30);code=124
 result=subprocess.CompletedProcess(command,code)
 for suffix in ('.json','.csv','.png'):
  source=runtime_out.with_suffix(suffix)
  if source.exists() and source.stat().st_mtime>=start:shutil.copy2(source,out.with_suffix(suffix))
 errors=[]
 if result.returncode!=0:errors.append('Game exit '+str(result.returncode))
 if not out.exists() or out.stat().st_mtime<start:errors.append('Fresh evidence missing')
 else:errors+=validate(json.loads(out.read_text()),a.mode)
 record={'command':command,'exit_code':result.returncode,'elapsed_seconds':time.time()-start,'errors':errors,'passed':not errors,'log':str(log.relative_to(ROOT))}
 (out.parent/('Runner_'+a.mode+'.json')).write_text(json.dumps(record,indent=2)+'\n')
 print(json.dumps(record,indent=2));return bool(errors)
if __name__=='__main__':raise SystemExit(main())
