"""Independently reload and validate the imported Wenmingmen Blueprint."""
import json
import sys
from pathlib import Path

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ImportWenmingmen import EAL, IMPORT_MANIFEST, REPORT, validate


def main():
    report_path = REPORT if REPORT.exists() else IMPORT_MANIFEST
    data = json.loads(Path(report_path).read_text(encoding="utf-8"))
    blueprint = EAL.load_asset(data["blueprint"])
    validate(blueprint, data)


if __name__ == "__main__":
    main()
