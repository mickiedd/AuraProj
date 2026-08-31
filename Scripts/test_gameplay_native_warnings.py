"""Negative warning classification cannot clear unrelated or drifting warnings."""
import json
import unittest

import gameplay_expansion as ge
import gameplay_native_warnings as nw


class NativeWarningTests(unittest.TestCase):
    def setUp(self):
        from test_gameplay_expansion import Day41Tests
        self.fixture = Day41Tests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.index, self.log, self.start, self.document = self.fixture.native_fixture(warnings=2)
        self.name = "Aura.RoleBattle.Day4.AuthorityRejection"
        self.document["tests"][0]["fullTestPath"] = self.name
        self.index.write_text(json.dumps(self.document), encoding="utf-8")
        self.warning = "LogAura: Warning: [DamageBoundary] Rejected damage: source or target ASC is invalid."
        self.begin = f"LogAutomationController: Display: Test Started. Name={{AuthorityRejection}} Path={{{self.name}}}"
        self.end = f"LogAutomationController: Display: Test Completed. Result={{Success}} Name={{AuthorityRejection}} Path={{{self.name}}}"
        self.lines = ["Found 1 automation tests based on 'Aura.RoleBattle.'", self.begin, self.warning, self.warning,
                      self.end, "**** TEST COMPLETE. EXIT CODE: 0 ****"]

    def result(self):
        self.log.write_text("\n".join(self.lines) + "\n", encoding="utf-8")
        return ge.native(self.index, self.log, 0, self.start, report_root=self.index.parent)

    def test_expected_negative_case_passes_and_retains_warning_lines(self):
        result = self.result()
        self.assertEqual(result["status"], "PASS", result)
        self.assertEqual(result["logWarningCount"], 2)
        self.assertEqual(len(result["warningLogLines"]), 2)
        self.assertEqual(result["warningClassification"]["classifiedCount"], 2)
        self.assertEqual(result["runtimeEntry"], "BLOCKED_RUNTIME_ENTRY")

    def test_extra_same_warning_does_not_match_expected_count(self):
        self.lines.insert(3, self.warning)
        self.assertEqual(self.result()["status"], "BLOCKED")

    def test_missing_expected_warning_blocks(self):
        del self.lines[3]
        self.assertEqual(self.result()["status"], "BLOCKED")

    def test_same_message_outside_test_is_not_accepted(self):
        self.lines.insert(0, self.lines.pop(2))
        result = self.result()
        self.assertEqual(result["status"], "BLOCKED")
        self.assertIn("WARNING_OUTSIDE_REVIEWED_TEST", [item["reason"] for item in result["warningClassification"]["issues"]])

    def test_asset_warning_inside_reviewed_test_is_not_accepted(self):
        self.lines[2] = "LogBlueprint: Warning: missing NodeGuid"
        self.assertEqual(self.result()["status"], "BLOCKED")

    def test_same_warning_in_unreviewed_test_is_not_accepted(self):
        name = "Aura.RoleBattle.Day41.Unreviewed"
        self.lines = [line.replace(self.name, name) for line in self.lines]
        self.document["tests"][0]["fullTestPath"] = name
        self.index.write_text(json.dumps(self.document), encoding="utf-8")
        self.assertEqual(self.result()["status"], "BLOCKED")

    def test_export_warning_count_must_match_log(self):
        self.document["tests"][0]["warnings"] = 1
        self.index.write_text(json.dumps(self.document), encoding="utf-8")
        result = self.result()
        self.assertEqual(result["status"], "BLOCKED")
        self.assertIn("WARNING_COUNT_EXPORT_MISMATCH", [item["reason"] for item in result["warningClassification"]["issues"]])

    def test_missing_start_cannot_attribute_warning(self):
        del self.lines[1]
        self.assertEqual(self.result()["status"], "BLOCKED")

    def test_nested_start_does_not_clear_ambiguous_attribution(self):
        self.lines.insert(2, self.begin)
        self.assertEqual(self.result()["status"], "BLOCKED")

    def test_error_never_accepted_as_negative_case_warning(self):
        self.lines[2] = self.warning.replace("Warning:", "Error:")
        self.assertEqual(self.result()["status"], "FAIL")

    def test_null_world_and_attachment_warnings_are_not_allowed(self):
        for message in ("LogScript: Warning: Script Msg: A null object was passed as a world context object.",
                        "LogSkinnedMeshComp: Warning: GetSocketInfoByName(WeaponHandSocket): No SkeletalMesh"):
            self.lines[2] = message
            self.assertEqual(self.result()["status"], "BLOCKED")


if __name__ == "__main__":
    unittest.main()
