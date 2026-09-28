"""Failure-focused tests for the headless Unreal exit-code normalizer."""
import json
import tempfile
import unittest
from pathlib import Path

from run_canton_district_check import inspect_evidence


NORMAL_LOG = "\n".join([
    '[0]LogAuraEditor: Error: RegisterMenus failed: no known ToolMenus path accepted the button.',
    '[0]LogAutomationTest: Error: Locale is "C.UTF-8" but should be "C". Did something call setlocale()?',
    '[9]MapCheck: Map check complete: 0 Error(s), 0 Warning(s), took 1ms to complete.',
    '[10]LogExit: Exiting.',
    'Log file closed, 09/28/26 09:00:00',
])
GOOD_REVIEW = {"passed": True, "errors": [], "road_collision_checks": 100,
               "max_road_long_edge_error_cm": .024}


class RunnerChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name)
        self.result = root / "result.json"
        self.log = root / "run.log"
        self.write()

    def write(self, result=GOOD_REVIEW, log=NORMAL_LOG):
        self.result.write_text(json.dumps(result))
        self.log.write_text(log)

    def inspect(self, code=1, started_ns=0):
        return inspect_evidence("review", code, self.result, self.log, started_ns)

    def test_normalizes_only_complete_known_ue_quit(self):
        self.assertEqual(self.inspect(), [])

    def test_missing_and_stale_evidence_fail(self):
        self.result.unlink()
        self.assertTrue(self.inspect())
        self.write()
        self.assertTrue(self.inspect(started_ns=self.result.stat().st_mtime_ns + 1))

    def test_json_mapcheck_and_validation_failures(self):
        self.result.write_text("{")
        self.assertTrue(self.inspect())
        self.write(result={**GOOD_REVIEW, "passed": False})
        self.assertTrue(self.inspect())
        self.write(log=NORMAL_LOG.replace("0 Error(s)", "1 Error(s)"))
        self.assertTrue(self.inspect())

    def test_crash_unknown_error_and_exit_code_fail(self):
        self.write(log=NORMAL_LOG + "\nLogPython: Error: traceback")
        self.assertTrue(self.inspect())
        self.write(log=NORMAL_LOG.replace("LogExit: Exiting.", "Fatal error"))
        self.assertTrue(self.inspect())
        self.write()
        self.assertTrue(self.inspect(code=2))

    def test_seam_requires_a_distinct_complete_joint_measurement(self):
        good = {"measurement": "adjacent principal slab top-corner vertical step",
                "road_joint_comparisons": 198, "max_road_joint_step_cm": 1.33}
        self.write(result=good)
        self.assertEqual(inspect_evidence("seam", 1, self.result, self.log, 0), [])
        self.write(result={**good, "road_joint_comparisons": 197})
        self.assertTrue(inspect_evidence("seam", 1, self.result, self.log, 0))

    def test_traversal_preserves_closed_gate_obstruction(self):
        route = {"nav_corner_floor_samples": [{"floor_z_cm": 800}]}
        good = {"route_count": 7, "all_route_checks_pass": True,
                "pawn_traversal_performed": False, "routes": [route] * 7,
                "closed_gate_transit_diagnostic": {"nav_detour_ratio": 2.58}}
        self.write(result=good)
        self.assertEqual(inspect_evidence("traversal", 1, self.result, self.log, 0), [])
        self.write(result={**good, "closed_gate_transit_diagnostic": {"nav_detour_ratio": 1.0}})
        self.assertTrue(inspect_evidence("traversal", 1, self.result, self.log, 0))


if __name__ == "__main__":
    unittest.main()
