#!/usr/bin/env bash
# Drive the staged Wuxianmen V5 capture harness one view per editor invocation.
set -u
cd "$(dirname "$0")/.."
SPEC=Saved/RawModelImport/V5/Wuxianmen_V5-reftune2-stage.json

write_spec() { python -c "
import json,pathlib,sys
d={'action':sys.argv[1]}
if len(sys.argv)>2: d['variant']=sys.argv[2]
if len(sys.argv)>3: d['view']=sys.argv[3]
pathlib.Path('$SPEC').write_text(json.dumps(d))
" "$@"; }

run() { python Scripts/remote_run.py Scripts/CaptureV5WuxianmenRefTune2Stage.py 2>&1 | grep -o "REFTUNE2_STAGE_[A-Z]*.*" | head -1; }

VARIANT="$1"; shift
write_spec setup "$VARIANT"; echo "setup -> $(run)"
for v in "$@"; do
  write_spec view "$VARIANT" "$v"
  echo "$v -> $(run)"
done
write_spec teardown "$VARIANT"; echo "teardown -> $(run)"
