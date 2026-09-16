"""Run the Wuxianmen AAA-redone close visual capture."""
import runpy


module = runpy.run_path(
    "C:/Git/AuraProj/Scripts/CaptureWuxianmenAAARedone.py",
    run_name="wuxianmen_capture_module",
)
module["capture"]("close")
