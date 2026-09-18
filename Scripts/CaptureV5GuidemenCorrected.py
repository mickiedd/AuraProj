"""Recapture corrected Guidemen views sequentially, one per invocation."""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from CaptureV5Buildings import ROOT, capture


STATE = ROOT / "Guidemen-upright-reimport-capture-state.json"
VIEWS = ("front", "rear", "side", "close")


def main():
    state = json.loads(STATE.read_text(encoding="utf-8")) if STATE.exists() else {"next": 0}
    index = state["next"]
    if index >= len(VIEWS):
        print("V5_GUIDEMEN_RECAPTURE_COMPLETE", len(VIEWS))
        return
    view = VIEWS[index]
    capture(0, view)
    state["next"] = index + 1
    STATE.write_text(json.dumps(state, indent=2), encoding="utf-8")
    print("V5_GUIDEMEN_RECAPTURE_STEP", view, state["next"])


if __name__ == "__main__":
    main()
