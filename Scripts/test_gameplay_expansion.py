#!/usr/bin/env python3
"""Behavioral Day 41 tooling tests. Synthetic receipts never authorize runtime."""
from __future__ import annotations

import argparse
import copy
from datetime import datetime, timedelta, timezone
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

import gameplay_expansion as ge


ROOT = Path(__file__).resolve().parents[1]


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


class Day41Tests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="aura-day41-tests-")
        self.addCleanup(self.temp.cleanup)
        # macOS exposes the default temporary root through /var -> /private/var.
        # Resolve that host alias so path-safety tests exercise their intended
        # fixture children instead of failing on the operating system's alias.
        self.root = Path(self.temp.name).resolve()
        self.repo = self.root / "repo"
        self.repo.mkdir()
        self.scope = ge.strict_json((ROOT / ge.SCOPE_PATH).read_bytes(), "scope fixture")

    def raises_code(self, code, fn):
        with self.assertRaises(ge.EvidenceError) as context:
            fn()
        self.assertEqual(context.exception.code, code)

    def fixture_repo(self):
        for path in (ge.LEGACY_SCOPE, ge.LEGACY_CONTENT):
            target = self.repo / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((ROOT / path).read_bytes())
        for args in (("init",), ("config", "user.name", "Synthetic Day41 Test"),
                     ("config", "user.email", "synthetic@example.invalid"), ("config", "core.autocrlf", "false"),
                     ("add", "."), ("commit", "-m", "synthetic baseline")):
            subprocess.run(["git", "-C", str(self.repo), *args], check=True, capture_output=True)
        head = ge.git(self.repo, "rev-parse", "HEAD").decode().strip()
        scope_path = self.repo / ge.SCOPE_PATH
        write_json(scope_path, self.scope)
        return head, scope_path

    def candidate(self, head="a" * 40):
        return {
            "schemaVersion": 2, "scopeRevision": "playable-candidate-v1", "sourceRevision": head,
            "runId": "synthetic-only", "status": "PASS", "localDisposition": "PASS", "externalDisposition": "BLOCKED",
            "scopeManifestSha256": self.scope["legacyScope"]["sha256"],
            "contentManifestSha256": self.scope["legacyContentManifest"]["sha256"], "packageSha256": "b" * 64,
            "draftPath": "draft.json", "soakPath": "soak.json", "laneEvidencePath": "lanes.json",
            "finalFastPath": "fast.json", "finalizedAtUtc": "2026-08-31T00:00:00Z", "immutable": True,
        }

    def bound(self, candidate):
        return {key: candidate[key] for key in ge.BINDING}

    def ref(self, path, root="evidence"):
        data = path.read_bytes()
        return {"root": root, "path": path.relative_to(self.root).as_posix(), "sizeBytes": len(data), "sha256": ge.digest(data)}

    def native_fixture(self, warnings=0):
        start = (datetime.now(timezone.utc) - timedelta(seconds=1)).isoformat()
        export = self.root / "native"
        index = export / "index.json"
        log = self.root / "native.log"
        name = "Aura.RoleBattle.Day21.Scope.Contract"
        document = {"succeeded": 0 if warnings else 1, "succeededWithWarnings": 1 if warnings else 0,
                    "failed": 0, "notRun": 0, "inProcess": 0,
                    "tests": [{"fullTestPath": name, "state": "Success", "warnings": warnings, "errors": 0, "duration": 0.01}]}
        write_json(index, document)
        log.write_text("Found 1 automation tests based on 'Aura.RoleBattle.'\n"
                       f"Test Completed. Result={{Success}} Name={{Scope}} Path={{{name}}}\n"
                       "**** TEST COMPLETE. EXIT CODE: 0 ****\n", encoding="utf-8")
        return index, log, start, document

    def package_fixture(self):
        candidate = self.candidate()
        entries = []
        extracted = self.root / "package" / "extracted"
        for relative, target, kind in (("client/Aura.exe", "Aura", "launcher"),
                                       ("client/Aura/Binaries/Aura.exe", "Aura", "executable"),
                                       ("client/content.pak", "Aura", "container"),
                                       ("server/AuraServer.exe", "AuraServer", "launcher"),
                                       ("server/Aura/Binaries/AuraServer.exe", "AuraServer", "executable")):
            path = extracted / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(f"SYNTHETIC UNIT TEST ONLY: {relative}".encode())
            entries.append({"path": relative, "sizeBytes": path.stat().st_size, "sha256": ge.digest(path.read_bytes()),
                            "kind": kind, "target": target, "configuration": "Development"})
        entries.sort(key=lambda entry: entry["path"].casefold())
        manifest = {key: candidate[key] for key in ge.BINDING[:-1]}
        manifest.update({"schemaVersion": 1, "kind": "PlayableCandidatePackageInventory", "engineVersion": "5.5.4",
                         "createdAtUtc": "2026-08-30T00:00:00Z", "entries": entries})
        data = json.dumps(manifest).encode()
        candidate["packageSha256"] = ge.digest(data)
        receipt = self.bound(candidate)
        receipt.update({"schemaVersion": 1, "kind": "PlayableCandidateBuildReceipt", "workingTreeStatusBefore": "",
                        "workingTreeStatusAfter": "", "packageManifestSha256": ge.digest(data),
                        "packageIdentityKind": "manifest-bytes-v1", "packageSubdirectory": "extracted"})
        identity = {"kind": "manifest-bytes-v1", "sha256": candidate["packageSha256"], "archive": None}
        roots = {"evidence": self.root, "package": self.root / "package"}
        return manifest, receipt, identity, candidate, data, roots

    def test_scope_required_fields_and_real_constants(self):
        ge.validate_scope(self.scope)
        for key in ("gameplayProfile", "lanes", "budgets", "baselineEvidence"):
            altered = copy.deepcopy(self.scope)
            del altered[key]
            self.raises_code("MISSING_FIELD", lambda: ge.validate_scope(altered))

    def test_scope_wrong_lane_host_role_order_rejected(self):
        self.scope["lanes"][5]["roles"] = ["Aura", "BungeeMan"]
        self.raises_code("LANE_MISMATCH", lambda: ge.validate_scope(self.scope))

    def test_scope_duplicate_lane_rejected(self):
        self.scope["lanes"][1] = copy.deepcopy(self.scope["lanes"][0])
        self.raises_code("LANE_MISMATCH", lambda: ge.validate_scope(self.scope))

    def test_scope_boolean_budget_and_float_seed_rejected(self):
        self.scope["budgets"]["serverHz"] = True
        self.raises_code("INVALID_TYPE", lambda: ge.validate_scope(self.scope))
        self.scope["budgets"]["serverHz"] = 30
        self.scope["seeds"]["Core"][0] = 41001.0
        self.raises_code("SEED_MISMATCH", lambda: ge.validate_scope(self.scope))

    def test_nested_scope_unknown_fields_and_resolution_types(self):
        for key in ("features", "authority", "budgets"):
            altered = copy.deepcopy(self.scope)
            altered[key]["inventedFutureOverride"] = True
            self.raises_code("UNKNOWN_FIELD", lambda: ge.validate_scope(altered))
        self.scope["budgets"]["resolution"] = [1920.0, 1080]
        self.raises_code("BUDGET_MISMATCH", lambda: ge.validate_scope(self.scope))

    def test_strict_json_duplicate_nonfinite_and_invalid_utf8(self):
        for data, code in ((b'{"a":1,"a":2}', "DUPLICATE_JSON_KEY"),
                           (b'{"a":{"x":1,"x":2}}', "DUPLICATE_JSON_KEY"),
                           (b'{"a":NaN}', "NONFINITE_JSON"), (b'{"a":1e999}', "NONFINITE_JSON"),
                           (b'\xff', "INVALID_JSON")):
            self.raises_code(code, lambda d=data: ge.strict_json(d, "synthetic"))

    def test_legacy_powershell_utf8_bom_accepted(self):
        self.assertEqual(ge.strict_json(b'\xef\xbb\xbf{"a":1}', "legacy"), {"a": 1})

    def test_unsafe_paths_and_runids_rejected(self):
        for path in ("../escape", "A/../x", "C:/file", "//server/file", "a//b", "a:stream", "NUL.txt", "a.", "a/*"):
            self.raises_code("UNSAFE_PATH", lambda p=path: ge.relative_path(p))
        for run_id in ("../x", "has space", "", ".", "..", "x" * 97, "CON", "NUL.json", "COM1", "run."):
            self.raises_code("UNSAFE_RUN_ID", lambda r=run_id: ge.validate_run_id(r))

    def test_containment_prefix_collision_rejected(self):
        self.raises_code("UNSAFE_PATH", lambda: ge.contained(self.root / "evidence", self.root / "evidence-escape" / "data.json"))

    def test_symlink_root_rejected(self):
        target = self.root / "target"
        target.mkdir()
        link = self.root / "linked"
        try:
            link.symlink_to(target, target_is_directory=True)
        except OSError:
            if os.name != "nt":
                self.skipTest("OS does not permit symlink creation")
            env = dict(os.environ, AURA_TEST_JUNCTION=str(link), AURA_TEST_TARGET=str(target))
            result = subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-Command",
                                     "New-Item -ItemType Junction -Path $env:AURA_TEST_JUNCTION -Target $env:AURA_TEST_TARGET -ErrorAction Stop | Out-Null"],
                                    capture_output=True, env=env, timeout=15)
            self.assertEqual(result.returncode, 0, result.stderr.decode(errors="replace"))
        self.raises_code("UNSAFE_PATH", lambda: ge.root_path(link))
        self.raises_code("UNSAFE_PATH", lambda: ge.root_path(link / "missing-child"))
        if os.name == "nt":
            # Remove only the checked run-owned junction/symlink, not its target.
            self.assertEqual(link.parent, self.root)
            os.rmdir(link)

    def test_file_hash_tamper_and_missing(self):
        path = self.root / "evidence.json"
        path.write_bytes(b'{"synthetic":true}')
        ref = self.ref(path)
        ge.read_ref(ref, {"evidence": self.root}, "fixture")
        path.write_bytes(b'{"synthetic":false}')
        self.raises_code("EVIDENCE_HASH_MISMATCH", lambda: ge.read_ref(ref, {"evidence": self.root}, "fixture"))
        path.unlink()
        self.raises_code("EVIDENCE_MISSING", lambda: ge.read_ref(ref, {"evidence": self.root}, "fixture"))

    def test_streaming_hash_reads_large_artifact(self):
        path = self.root / "trace.utrace"
        with path.open("wb") as handle:
            handle.seek(65 * 1024 * 1024 - 1)
            handle.write(b"x")
        size, sha = ge.file_digest(path)
        self.assertEqual(size, 65 * 1024 * 1024)
        self.assertEqual(len(sha), 64)
        self.raises_code("RECORD_TOO_LARGE", lambda: ge.file_bytes(path))

    def test_actual_legacy_stub_schema_shape_rejected(self):
        # Mirrors the schema actually found in review-day40/candidate.json;
        # no test depends on an ignored historical Saved directory existing.
        stub = {"schemaVersion": 1, "scopeRevision": "playable-candidate-v1", "days": "40", "mode": "Both",
                "testsRun": 1, "failures": [], "passed": True, "durationSeconds": 0.0, "day": 40,
                "runId": "review-day40", "validationMode": "local-contract", "scopeValidatorExit": 0,
                "contentValidatorExit": 0, "packagedEvidence": "not executed by day contract runner"}
        self.raises_code("LEGACY_STUB_REJECTED", lambda: ge.validate_candidate(stub))

    def test_candidate_bool_string_and_missing_hash_rejected(self):
        candidate = self.candidate()
        ge.validate_candidate(candidate)
        candidate["immutable"] = "true"
        self.raises_code("CANDIDATE_NOT_FINAL", lambda: ge.validate_candidate(candidate))
        candidate["immutable"] = True
        candidate["packageSha256"] = ""
        self.raises_code("INVALID_HASH", lambda: ge.validate_candidate(candidate))

    def test_fast_draft_cannot_be_packaged_candidate(self):
        candidate = self.candidate()
        draft = self.bound(candidate)
        draft.update({"schemaVersion": 2, "stage": "Fast", "mode": "Both", "role": "Both", "status": "PASS",
                      "localDisposition": "PASS", "localContractStatus": "PASS", "packagedStatus": "NOT_RUN",
                      "workingTreeStatus": "", "blockers": []})
        self.raises_code("DRAFT_NOT_PACKAGED_PASS", lambda: ge.validate_draft(draft, candidate))

    def test_cross_record_run_mismatch(self):
        candidate = self.candidate()
        record = self.bound(candidate)
        record["runId"] = "other-run"
        self.raises_code("BINDING_MISMATCH", lambda: ge.validate_binding(record, candidate, "soak"))

    def test_aggregate_soak_and_duplicate_cycles_rejected(self):
        candidate = self.candidate()
        soak = self.bound(candidate)
        soak.update({"schemaVersion": 2, "status": "PASS", "passed": True, "cyclesPerLane": 10,
                     "lanes": [{"laneId": lane} for lane in ge.OLD_LANES]})
        self.raises_code("SOAK_CYCLES_INCOMPLETE", lambda: ge.validate_lanes(soak, candidate, True))
        for lane in soak["lanes"]:
            lane["cycles"] = [{"cycleId": 1}] * 10
        self.raises_code("SOAK_CYCLES_INCOMPLETE", lambda: ge.validate_lanes(soak, candidate, True))

    def test_frozen_without_measured_inputs_rejected(self):
        self.scope.update({"phase": "Frozen", "sourceRevisionAtFreeze": "a" * 40, "blockers": []})
        self.raises_code("FROZEN_EVIDENCE_MISSING", lambda: ge.validate_scope(self.scope))

    def test_draft_never_ready_and_legacy_bytes_unchanged(self):
        _, scope_path = self.fixture_repo()
        before = {path: (self.repo / path).read_bytes() for path in (ge.LEGACY_SCOPE, ge.LEGACY_CONTENT)}
        result = ge.audit(self.repo, scope_path)
        self.assertEqual(result["status"], "BLOCKED")
        self.assertEqual(result["runtimeEntry"], "BLOCKED_RUNTIME_ENTRY")
        self.assertEqual(result["inputPaths"]["scope"], str(scope_path))
        self.assertEqual(result["roots"], {"repo": str(self.repo), "evidence": None, "package": None})
        self.assertIn("VALIDATOR_CAPABILITY_MISSING", {check["reasonCode"] for check in result["checks"]})
        self.assertFalse(result["capabilities"]["publicInputCheckpointSemantics"])
        for path, data in before.items():
            self.assertEqual((self.repo / path).read_bytes(), data)

    def test_actual_git_ancestor_and_nonancestor(self):
        first, _ = self.fixture_repo()
        (self.repo / "second.txt").write_text("second", encoding="utf-8")
        ge.git(self.repo, "add", ".")
        ge.git(self.repo, "commit", "-m", "second")
        second = ge.git(self.repo, "rev-parse", "HEAD").decode().strip()
        ge.commit_ancestor(self.repo, first, second)
        self.raises_code("SOURCE_NOT_ANCESTOR", lambda: ge.commit_ancestor(self.repo, second, first))
        self.raises_code("INVALID_REVISION", lambda: ge.commit_ancestor(self.repo, first[:7], second))

    def test_git_timeout_takes_precedence_over_failure_and_blocked(self):
        report = ge.Report("audit")
        report.check("Malformed", lambda: ge.require(False, "INVALID_TYPE", "synthetic bad input"))
        report.block("Missing", "EVIDENCE_MISSING", "synthetic missing input")
        with mock.patch.object(ge.subprocess, "run", side_effect=subprocess.TimeoutExpired("git", 20)):
            report.check("Git", lambda: ge.git(self.repo, "rev-parse", "HEAD"))
        result = report.finish()
        self.assertEqual(result["status"], "TIMEOUT")
        self.assertEqual(result["reasonCode"], "GIT_TIMEOUT")
        self.assertEqual(ge.result_exit(result), 3)

    def test_cli_preserves_timeout_status_without_real_wait(self):
        output = self.root / "timeout.json"
        with mock.patch.object(ge, "audit", side_effect=ge.EvidenceError("GIT_TIMEOUT", "synthetic timeout", "TIMEOUT")):
            exit_code = ge.main(["audit", "--repo-root", str(self.repo), "--scope", str(self.repo / "scope.json"), "--output", str(output)])
        self.assertEqual(exit_code, 3)
        self.assertEqual(json.loads(output.read_text(encoding="utf-8"))["status"], "TIMEOUT")

    def test_package_actual_bytes_and_unlisted_file(self):
        values = self.package_fixture()
        ge.validate_package(*values)
        extra = values[-1]["package"] / "extracted" / "unlisted.dll"
        extra.write_bytes(b"unlisted")
        self.raises_code("PACKAGE_INVENTORY_INCOMPLETE", lambda: ge.validate_package(*values))

    def test_package_tamper_and_dirty_receipt(self):
        values = self.package_fixture()
        path = values[-1]["package"] / "extracted" / values[0]["entries"][0]["path"]
        path.write_bytes(b"changed")
        self.raises_code("EVIDENCE_HASH_MISMATCH", lambda: ge.validate_package(*values))
        values[1]["workingTreeStatusBefore"] = " M Source/Aura.cpp"
        self.raises_code("DIRTY_PACKAGE_SOURCE", lambda: ge.validate_package(*values))

    def test_package_identity_never_reinterpreted(self):
        values = self.package_fixture()
        values[2]["kind"] = "legacy-unknown"
        values[1]["packageIdentityKind"] = "legacy-unknown"
        self.raises_code("PACKAGE_IDENTITY_UNPROVEN", lambda: ge.validate_package(*values))

    def test_package_extensions_reject_boolean_and_array(self):
        for invalid in (False, []):
            manifest, receipt, identity, candidate, _, roots = self.package_fixture()
            manifest["extensions"] = invalid
            data = json.dumps(manifest).encode()
            candidate["packageSha256"] = ge.digest(data)
            receipt.update(self.bound(candidate))
            receipt["packageManifestSha256"] = ge.digest(data)
            identity["sha256"] = ge.digest(data)
            self.raises_code("INVALID_TYPE", lambda: ge.validate_package(manifest, receipt, identity, candidate, data, roots))

    def test_bundle_extensions_reject_boolean_before_references(self):
        _, scope_path = self.fixture_repo()
        bundle = {key: None for key in ge.BUNDLE_REFS}
        bundle.update({"schemaVersion": 1, "kind": "GameplayBaselineEvidence", "packageIdentity": {}, "extensions": False})
        path = self.root / "bundle.json"
        write_json(path, bundle)
        result = ge.audit(self.repo, scope_path, path)
        self.assertEqual(result["status"], "FAIL")
        self.assertEqual(next(check for check in result["checks"] if check["name"] == "BundleSchema")["reasonCode"], "INVALID_TYPE")

    def test_utc_dotnet_roundtrip_fraction_truncates_without_rounding(self):
        for fraction, microsecond in (("0000000", 0), ("1234567", 123456), ("9999999", 999999)):
            for suffix in ("Z", "+00:00"):
                with self.subTest(fraction=fraction, suffix=suffix):
                    self.assertEqual(ge.utc(f"2026-08-31T01:02:03.{fraction}{suffix}", "timestamp"),
                                     datetime(2026, 8, 31, 1, 2, 3, microsecond, timezone.utc))

    def test_utc_dotnet_roundtrip_invalid_date_or_offset_rejected(self):
        for value in ("2026-02-30T01:02:03.1234567Z", "2026-08-31T24:02:03.1234567Z",
                      "2026-08-31T01:02:03.1234567+08:00", "2026-08-31T01:02:03.1234567-01:00",
                      "2026-08-31T01:02:03.1234567+24:00", "2026-08-31T01:02:03.1234567+00:0",
                      "2026-08-31T01:02:03.1234567"):
            with self.subTest(value=value):
                self.raises_code("INVALID_TIMESTAMP", lambda: ge.utc(value, "timestamp"))

    def test_candidate_dotnet_roundtrip_finalization_timestamp(self):
        candidate = self.candidate()
        candidate["finalizedAtUtc"] = "2026-08-31T01:02:03.1234567Z"
        ge.validate_candidate(candidate)
        for value in ("2026-02-30T01:02:03.1234567Z", "2026-08-31T01:02:03.1234567+08:00"):
            candidate["finalizedAtUtc"] = value
            self.raises_code("INVALID_TIMESTAMP", lambda: ge.validate_candidate(candidate))

    def test_native_dotnet_roundtrip_start_timestamp(self):
        index, log, start, _ = self.native_fixture()
        start_time = datetime.fromisoformat(start)
        dotnet_start = start_time.strftime("%Y-%m-%dT%H:%M:%S.%f") + "7Z"
        result = ge.native(index, log, 0, dotnet_start)
        self.assertEqual(result["status"], "PASS")
        self.assertEqual(result["runnerStartedAtUtc"], start_time.isoformat())
        self.assertEqual(result["runtimeEntry"], "BLOCKED_RUNTIME_ENTRY")

    def test_native_no_quit_queue_empty_completion(self):
        index, log, start, _ = self.native_fixture()
        handshake = ("LogAutomationCommandLine: Display: ...Automation Test Queue Empty 1 tests performed.\n"
                     "LogExit: Display: **** TestExit: Automation Test Queue Empty ****\n"
                     "LogWindows: FPlatformMisc::RequestExitWithStatus(1, 0, FEngineLoop::Tick.GScopedTestExit)\n")
        log.write_text(log.read_text(encoding="utf-8").replace("**** TEST COMPLETE. EXIT CODE: 0 ****\n", handshake), encoding="utf-8")
        result = ge.native(index, log, 0, start)
        self.assertEqual(result["status"], "PASS")
        self.assertEqual(result["completionMode"], "QueueEmptyTestExit")
        self.assertEqual(result["observedTestCount"], 1)

    def test_native_mac_no_quit_queue_empty_completion(self):
        index, log, start, _ = self.native_fixture()
        handshake = ("LogAutomationCommandLine: Display: ...Automation Test Queue Empty 1 tests performed.\n"
                     "LogExit: Display: **** TestExit: Automation Test Queue Empty ****\n"
                     "LogMac: FPlatformMisc::RequestExit(1, FEngineLoop::Tick.GScopedTestExit)\n")
        log.write_text(log.read_text(encoding="utf-8").replace("**** TEST COMPLETE. EXIT CODE: 0 ****\n", handshake), encoding="utf-8")
        result = ge.native(index, log, 0, start)
        self.assertEqual(result["status"], "PASS")
        self.assertEqual(result["completionMode"], "QueueEmptyTestExit")
        self.assertEqual(result["observedTestCount"], 1)

    def test_native_no_quit_incomplete_conflicting_or_reordered_completion(self):
        handshake = ["LogAutomationCommandLine: Display: ...Automation Test Queue Empty 1 tests performed.\n",
                     "LogExit: Display: **** TestExit: Automation Test Queue Empty ****\n",
                     "LogWindows: FPlatformMisc::RequestExitWithStatus(1, 0, FEngineLoop::Tick.GScopedTestExit)\n"]
        mutations = [handshake[:i] + handshake[i + 1:] for i in range(3)]
        mutations += [handshake + [handshake[i]] for i in range(3)]
        mutations += [list(reversed(handshake)), [handshake[1], handshake[0], handshake[2]],
                      [handshake[0].replace("1 tests", "2 tests"), *handshake[1:]],
                      [handshake[0].replace("1 tests", "unknown tests"), *handshake[1:]],
                      [*handshake[:2], handshake[2].replace("(1, 0,", "(1, 1,")],
                      [*handshake[:2], handshake[2].replace("FEngineLoop::Tick.GScopedTestExit", "<NoCallSiteInfo>")],
                      handshake + ["**** TEST COMPLETE. EXIT CODE: 0 ****\n"]]
        for mutated in mutations:
            with self.subTest(handshake=mutated):
                index, log, start, _ = self.native_fixture()
                text = log.read_text(encoding="utf-8").replace("**** TEST COMPLETE. EXIT CODE: 0 ****\n", "".join(mutated))
                log.write_text(text, encoding="utf-8")
                self.assertEqual(ge.native(index, log, 0, start)["status"], "FAIL")

    def test_native_completion_before_test_result_rejected(self):
        index, log, start, _ = self.native_fixture()
        lines = log.read_text(encoding="utf-8").splitlines(keepends=True)
        log.write_text(lines[0] + lines[2] + lines[1], encoding="utf-8")
        self.assertEqual(ge.native(index, log, 0, start)["reasonCode"], "NATIVE_LOG_RESULT_MISMATCH")

    def test_native_fresh_success_and_warning_preservation(self):
        index, log, start, _ = self.native_fixture()
        result = ge.native(index, log, 0, start)
        self.assertEqual(result["status"], "PASS")
        self.assertEqual(result["completionMode"], "ExplicitTestComplete")
        self.assertEqual(result["runtimeEntry"], "BLOCKED_RUNTIME_ENTRY")
        self.assertEqual(result["inputPaths"], {"index": str(index), "log": str(log)})
        index, log, start, _ = self.native_fixture(warnings=2)
        result = ge.native(index, log, 0, start)
        self.assertEqual(result["status"], "BLOCKED")
        self.assertEqual(result["warningTestCount"], 1)
        self.assertEqual(result["toolingStatus"], "PASS")

    def test_native_empty_duplicate_and_unrelated_tests(self):
        for modification in ("empty", "duplicate", "namespace"):
            index, log, start, document = self.native_fixture()
            if modification == "empty":
                document["tests"] = []
            elif modification == "duplicate":
                document["tests"].append(copy.deepcopy(document["tests"][0]))
            else:
                document["tests"][0]["fullTestPath"] = "Other.Namespace.Test"
            write_json(index, document)
            self.assertEqual(ge.native(index, log, 0, start)["status"], "FAIL")

    def test_native_stale_and_child_failure(self):
        index, log, start, _ = self.native_fixture()
        self.assertEqual(ge.native(index, log, 7, start)["reasonCode"], "NATIVE_CHILD_FAILED")
        old = (datetime.now(timezone.utc) - timedelta(days=1)).timestamp()
        os.utime(index, (old, old))
        self.assertEqual(ge.native(index, log, 0, start)["reasonCode"], "STALE_NATIVE_REPORT")

    def test_native_aggregate_mismatch_failure_pending_and_bool_count(self):
        for field, value in (("succeeded", 2), ("failed", 1), ("notRun", 1), ("inProcess", 1), ("succeeded", True)):
            index, log, start, document = self.native_fixture()
            document[field] = value
            write_json(index, document)
            self.assertEqual(ge.native(index, log, 0, start)["status"], "FAIL")

    def test_native_discovery_completion_and_per_test_log_match(self):
        for replacement in ("discovery", "completion", "test", "crash"):
            index, log, start, _ = self.native_fixture()
            text = log.read_text(encoding="utf-8")
            if replacement == "discovery":
                text = text.replace("Found 1", "Found 0")
            elif replacement == "completion":
                text = text.replace("EXIT CODE: 0", "EXIT CODE: 1")
            elif replacement == "test":
                text = text.replace("Result={Success}", "Result={Fail}")
            else:
                text += "Fatal error: synthetic failure\n"
            log.write_text(text, encoding="utf-8")
            self.assertEqual(ge.native(index, log, 0, start)["status"], "FAIL")

    def test_native_startup_warning_and_error_not_silently_passed(self):
        for severity, expected in (("Warning", "BLOCKED"), ("Error", "FAIL")):
            index, log, start, _ = self.native_fixture()
            log.write_text(f"LogSynthetic: {severity}: synthetic startup finding\n" + log.read_text(encoding="utf-8"), encoding="utf-8")
            result = ge.native(index, log, 0, start)
            self.assertEqual(result["status"], expected)
            self.assertEqual(result["nativeTestsStatus"], "PASS")
            self.assertEqual(result["logWarningCount"] if severity == "Warning" else result["logErrorCount"], 1)
            self.assertNotIn("synthetic startup finding", json.dumps(result))

    def test_output_exclusive_and_input_preserved(self):
        path = self.root / "result.json"
        ge.write_result(path, {"status": "BLOCKED"}, [])
        original = path.read_bytes()
        with self.assertRaises(FileExistsError):
            ge.write_result(path, {"status": "PASS"}, [])
        self.assertEqual(path.read_bytes(), original)
        self.raises_code("OUTPUT_COLLISION", lambda: ge.write_result(path, {}, [path]))

    def test_cli_audit_emits_blocked_json(self):
        _, scope_path = self.fixture_repo()
        output = self.root / "cli-output.json"
        process = subprocess.run([sys.executable, str(ROOT / "Scripts/gameplay_expansion.py"), "audit",
                                  "--repo-root", str(self.repo), "--scope", str(scope_path), "--output", str(output)],
                                 capture_output=True, text=True, timeout=30)
        self.assertEqual(process.returncode, 2, process.stderr + process.stdout)
        result = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual(result["runtimeEntry"], "BLOCKED_RUNTIME_ENTRY")
        self.assertEqual(result["status"], "BLOCKED")


def gameplay_binding():
    return {"runId":"final-run","sourceRevision":"a"*40,"scopeRevision":"gameplay-expansion-v1",
            "scopeManifestSha256":"b"*64,"contentManifestSha256":"c"*64,"packageSha256":"d"*64}


def playtest_fixture():
    binding=gameplay_binding(); participants=[]; sessions=[]
    for i in range(6):
        pid=f"P{i+1}"; initial=f"{pid}-initial"
        participants.append({"participantId":pid,"consent":True,"priorExposure":False,"initialSessionId":initial,
                             "objectiveSeconds":45,"augmentUnaided":True,"resupplyUnaided":True,"enjoyment":4,
                             "voluntaryReplay":i<4,"decisionAtUtc":"2026-09-01T10:00:00Z","scheduledAtUtc":"2026-09-01T11:00:00Z",
                             "decisionSequence":1,"taskSequence":2,"consentRecordSha256":format(i+1,'064x')})
        for kind in ("Initial","SecondRole","ComparisonA","ComparisonB"):
            sessions.append({"sessionId":initial if kind=="Initial" else f"{pid}-{kind}","participantId":pid,
                             "role":"Aura" if kind in ("Initial","ComparisonA") else "BungeeMan","kind":kind,
                             "contentManifestSha256":binding["contentManifestSha256"]})
    return {**binding,"schemaVersion":1,"producer":"AuraConsentedPlaytestImporter","evidenceClass":"HUMAN_OBSERVATION","createdAtUtc":"2026-09-01T12:00:00Z","participants":participants,"sessions":sessions,
            "offers":[{"offerId":f"offer-{i}","eligible":True,"selected":i<5} for i in range(10)],
            "privacy":{"containsRawIdentity":False,"containsVoice":False,"containsVideo":False,"videoConsent":False}}


class Day59Tests(unittest.TestCase):
    def setUp(self): self.record=playtest_fixture();self.binding=gameplay_binding()
    def code(self, expected, fn):
        with self.assertRaises(ge.EvidenceError) as c: fn()
        self.assertEqual(c.exception.code,expected)
    def test_comparable_offer_denominator(self):
        self.assertEqual(ge.validate_playtest(self.record,self.binding)["eligibleOfferCount"],10)
        self.record["offers"].pop();self.code("PLAYTEST_OFFER_DENOMINATOR",lambda:ge.validate_playtest(self.record,self.binding))
    def test_playtest_artifact_binding(self):
        self.record["contentManifestSha256"]="e"*64;self.code("BINDING_MISMATCH",lambda:ge.validate_playtest(self.record,self.binding))
        self.record=playtest_fixture();self.record["sessions"][1]["sessionId"]=self.record["sessions"][0]["sessionId"]
        self.code("PLAYTEST_SESSION_INVALID",lambda:ge.validate_playtest(self.record,self.binding))
    def test_first_use_cohort_is_fresh(self):
        self.record["participants"][0]["priorExposure"]=True;self.code("PLAYTEST_COHORT_NOT_FRESH",lambda:ge.validate_playtest(self.record,self.binding))
    def test_voluntary_replay_before_scheduled_tasks(self):
        self.record["participants"][0]["decisionAtUtc"]="2026-09-01T12:00:00Z";self.code("PLAYTEST_REPLAY_COERCED",lambda:ge.validate_playtest(self.record,self.binding))
        self.record=playtest_fixture();self.record["participants"][0]["decisionSequence"]=2;self.record["participants"][0]["taskSequence"]=2
        self.code("PLAYTEST_REPLAY_COERCED",lambda:ge.validate_playtest(self.record,self.binding))
    def test_usability_thresholds(self):
        self.assertEqual(ge.validate_playtest(self.record,self.binding)["status"],"PASS")
        for row in self.record["participants"][:2]:row["augmentUnaided"]=False
        self.assertEqual(ge.validate_playtest(self.record,self.binding)["status"],"NEEDS_ITERATION")


def final_bundle():
    binding=gameplay_binding()
    samples=[{"runId":f"perf-{i}","executableSha256":format(i+10,'064x'),"packageSha256":binding["packageSha256"],"map":"GameplayExpansion","scenario":"BusyCombat","rhi":"Metal","resolution":[1920,1080],"quality":"Medium","rendered":True,"warmupSeconds":300,"sampleSeconds":600,"traceSha256":format(i+20,'064x')} for i in range(3)]
    perf={**binding,"schemaVersion":1,"status":"PASS","producer":"AuraPerformanceRunner","evidenceClass":"RUNTIME_RENDERED","createdAtUtc":"2026-09-01T12:00:00Z","candidateIdentity":binding["packageSha256"],"baselineIdentity":"e"*64,
          "rendered":True,"traceSha256":"f"*64,"hardwareFingerprint":"hw","settingsFingerprint":"1080p-medium","cycles":10,
          "memoryGrowthPercent":4,"actorCountBefore":8,"actorCountAfter":8,"timerCountBefore":3,"timerCountAfter":3,"samples":samples}
    bundle={**binding,"schemaVersion":1,"cleanSource":True,"technicalStatus":"PASS","playtest":playtest_fixture(),"performance":perf,
            "secondAuthorStatus":"PASS","nativeGroups":{f"Day{d}":[f"Aura.Gameplay.Day{d}.Contract"] for d in range(42,60)},
            "standardRows":[f"{t}|{l}|{lane}" for t in ("Assault","RescueRelay","Sabotage") for l in ("arrangement_a","arrangement_b") for lane in ge.LANES],
            "mutatorRows":[f"{t}|{l}|{m}" for t in ("Assault","RescueRelay","Sabotage") for l in ("arrangement_a","arrangement_b") for m in ("RestlessPatrols","VolatileVents")],
            "lifecycleLanes":list(ge.LANES),"legacyLanes":list(ge.OLD_LANES),
            "soakCycles":[{"cycleId":f"{family}-{i}","family":family,"orphanGrowth":0,"rewardDuplicates":0,"packageSha256":binding["packageSha256"]} for family in ("Solo","Listen","Dedicated") for i in range(10)],
            "openP0P1":0,"externalProviderStatus":"BLOCKED","validatorVersion":"gameplay-finalizer-v1"}
    bundle["inputManifestSha256"]=ge.digest(json.dumps(bundle,sort_keys=True,separators=(",",":"),ensure_ascii=True,allow_nan=False).encode())
    return bundle


class Day60Tests(unittest.TestCase):
    def setUp(self): self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.output=Path(self.temp.name).resolve()/"candidate.json";self.bundle=final_bundle()
    def code(self, expected, fn):
        with self.assertRaises(ge.EvidenceError) as c: fn()
        self.assertEqual(c.exception.code,expected)
    def test_final_artifact_exact_binding(self):
        self.bundle["playtest"]["packageSha256"]="0"*64;self.code("BINDING_MISMATCH",lambda:ge.finalize_gameplay_candidate(self.bundle,self.output))
    def test_required_matrix_complete(self):
        self.bundle["standardRows"].pop();self.code("STANDARD_MATRIX_INCOMPLETE",lambda:ge.finalize_gameplay_candidate(self.bundle,self.output))
        self.bundle=final_bundle();self.bundle["standardRows"][0]="invented|row|lane";self.code("STANDARD_MATRIX_INCOMPLETE",lambda:ge.finalize_gameplay_candidate(self.bundle,self.output))
    def test_no_false_playability_pass(self):
        self.bundle["performance"]["status"]="BLOCKED";self.code("PERFORMANCE_NOT_PASS",lambda:ge.finalize_gameplay_candidate(self.bundle,self.output));self.assertFalse(self.output.exists())
    def test_human_evidence_is_uncoerced_and_current(self):
        self.bundle["playtest"]["participants"][0]["priorExposure"]=True;self.code("PLAYTEST_COHORT_NOT_FRESH",lambda:ge.finalize_gameplay_candidate(self.bundle,self.output))
    def test_final_metrics_do_not_borrow_baseline(self):
        self.bundle["performance"]["candidateIdentity"]=self.bundle["performance"]["baselineIdentity"];self.code("PERFORMANCE_IDENTITY_INVALID",lambda:ge.finalize_gameplay_candidate(self.bundle,self.output))
        self.bundle=final_bundle();self.bundle["performance"]["samples"][0]["rhi"]="NullRHI";self.code("PERFORMANCE_SAMPLE_INVALID",lambda:ge.finalize_gameplay_candidate(self.bundle,self.output))
    def test_immutable_final_record(self):
        ge.finalize_gameplay_candidate(self.bundle,self.output)
        with self.assertRaises(FileExistsError):ge.finalize_gameplay_candidate(self.bundle,self.output)
    def test_soak_cycle_and_cleanup_accounting(self):
        self.bundle["soakCycles"][0]["orphanGrowth"]=1;self.code("SOAK_CLEANUP_INVALID",lambda:ge.finalize_gameplay_candidate(self.bundle,self.output))
    def test_external_gate_independent(self):
        result=ge.finalize_gameplay_candidate(self.bundle,self.output);self.assertEqual(result["localGameplay"],"PASS");self.assertEqual(result["externalProvider"],"BLOCKED")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--day", type=int, default=41)
    parser.add_argument("--day41", action="store_const", const=41, dest="day")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    if args.day not in (41,59,60):
        result = {"schemaVersion": 1, "day": args.day, "status": "FAIL", "reasonCode": "UNSUPPORTED_DAY", "passed": False, "testsRun": 0}
        try:
            ge.write_result(args.output, result, [])
        except (OSError, ge.EvidenceError) as exc:
            print(str(exc), file=sys.stderr)
        return 1
    stream = io.StringIO()
    runner = unittest.TextTestRunner(stream=stream, verbosity=2)
    suite = unittest.defaultTestLoader.loadTestsFromTestCase({41:Day41Tests,59:Day59Tests,60:Day60Tests}[args.day])
    if args.day == 41:
        from test_gameplay_baseline_readers import BaselineReaderTests
        suite.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(BaselineReaderTests))
        from test_gameplay_native_warnings import NativeWarningTests
        suite.addTests(unittest.defaultTestLoader.loadTestsFromTestCase(NativeWarningTests))
    outcome = runner.run(suite)
    result = {"schemaVersion": 1, "day": args.day, "status": "PASS" if outcome.wasSuccessful() else "FAIL",
              "reasonCode": "TOOLING_TESTS_PASSED" if outcome.wasSuccessful() else "TOOLING_TESTS_FAILED",
              "passed": outcome.wasSuccessful(), "testsRun": outcome.testsRun,
              "failures": [{"test": str(test), "traceback": trace} for test, trace in outcome.failures + outcome.errors],
              "skipped": [{"test": str(test), "reason": reason} for test, reason in outcome.skipped],
              "runtimeEntry": "BLOCKED_RUNTIME_ENTRY", "fixtureClassification": "synthetic-tooling-tests-only"}
    try:
        ge.write_result(args.output, result, [])
    except (OSError, ge.EvidenceError) as exc:
        print(str(exc), file=sys.stderr)
        return 1
    print(stream.getvalue())
    return 0 if outcome.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
