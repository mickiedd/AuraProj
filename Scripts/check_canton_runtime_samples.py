"""Independently reconcile the raw runtime sample stream against its summary."""
import csv,json,math
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def check(summary,rows):
 errors=[]
 if len(rows)!=summary.get('frame_samples'):errors.append('Frame count differs from CSV')
 if not rows:return errors+['No raw samples']
 times=[float(r['elapsed_seconds']) for r in rows]
 if not all(b>a for a,b in zip(times,times[1:])):errors.append('Non-monotonic sample times')
 if not 60<=times[0]<61 or not 239<times[-1]<240:errors.append('Incomplete sampling interval')
 for column,key,q in [('frame_ms','p95_ms',.95),('frame_ms','p99_ms',.99),('gpu_ms','gpu_p95_ms',.95)]:
  values=sorted(float(r[column]) for r in rows if float(r[column])>0)
  if not values or not math.isclose(values[math.ceil(q*len(values))-1],summary.get(key,-1),abs_tol=.00001):errors.append('Percentile mismatch: '+key)
 if sum(float(r['gpu_ms'])>0 for r in rows)!=summary.get('gpu_samples'):errors.append('GPU count mismatch')
 if max(int(r['rss_bytes']) for r in rows)!=summary.get('rss_peak_bytes'):errors.append('RSS mismatch')
 if max(int(r['draw_calls']) for r in rows)!=summary.get('draw_calls_max'):errors.append('Draw-call mismatch')
 if sum(r['streaming_complete']=='0' for r in rows)!=summary.get('streaming_pending_samples'):errors.append('Streaming sample mismatch')
 return errors
if __name__=='__main__':
 out=ROOT/'QA/Canton_Continuation';summary=json.loads((out/'Runtime_performance.json').read_text())
 with (out/'Runtime_performance.csv').open() as f:rows=list(csv.DictReader(f))
 errors=check(summary,rows);result={'passed':not errors,'errors':errors,'rows':len(rows),'scope':'independent raw-sample reconciliation; runtime acceptance is separate'}
 (out/'Runtime_Sample_Audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result));raise SystemExit(bool(errors))
