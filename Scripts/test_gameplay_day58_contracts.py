#!/usr/bin/env python3
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
d=json.loads((ROOT/'Content/Config/GameplayPerformanceBudgets.json').read_text())
assert d['server']=={'targetHz':30,'gameThreadP95MsMax':25.0,'gameThreadP99MsMax':33.3,'schedulingP95MsMax':2.0}
assert d['sampling']=={'warmupSeconds':300,'sampleSeconds':600,'repeats':3,'cleanupCycles':10}
assert d['evidence']['renderedRequired'] and d['evidence']['traceRequired'] and d['evidence']['distinctBaselineAndCandidate']
print('Day58 static contracts: PASS (3/3)')
