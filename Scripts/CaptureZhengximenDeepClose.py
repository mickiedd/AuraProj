from pathlib import Path
import runpy

runpy.run_path(str(Path(__file__).with_name("CaptureGuangzhouLandmarkDeepAAA.py")), init_globals={"REQUESTED_GATE": "Zhengximen", "REQUESTED_VIEW": "close"})
