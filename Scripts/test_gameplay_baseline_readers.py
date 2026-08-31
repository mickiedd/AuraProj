"""Adversarial support-reader tests; all receipts here are synthetic fixtures."""
from __future__ import annotations

import copy
import io
import json
from pathlib import Path
import stat
import subprocess
import sys
import unittest
from unittest import mock
import warnings
import zipfile

import gameplay_expansion as ge
import gameplay_baseline_readers as br


class BaselineReaderTests(unittest.TestCase):
    def setUp(self):
        from test_gameplay_expansion import Day41Tests
        self.fixture = Day41Tests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.root = self.fixture.root
        self.candidate = self.fixture.candidate()
        self.preserved = {}
        self.refs = br.References({"evidence": self.root, "package": self.root / "package"}, self.preserved)
        self.source = self.file("producer.py", b"# synthetic receipt test; never executed\n")
        self.evidence = self.file("review-notes.md", b"SYNTHETIC: no human attestation or game run.\n")

    def file(self, name, data):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return {"root": "evidence", "path": name, "sha256": ge.digest(data), "sizeBytes": len(data)}

    def common(self, kind):
        return {**self.fixture.bound(self.candidate), "schemaVersion": 1, "kind": kind, "status": "PASS",
                "startedAtUtc": "2026-08-31T00:00:00Z", "completedAtUtc": "2026-08-31T00:10:00Z",
                "evidence": [self.evidence], "extensions": {"producer": {
                    "producerId": "AuraBaselineReceipt", "producerVersion": 1, "source": self.source}}}

    def fails(self, code, action):
        with self.assertRaises(ge.EvidenceError) as caught:
            action()
        self.assertEqual(caught.exception.code, code)

    def archive(self, variant=None):
        values = list(self.fixture.package_fixture())
        manifest, receipt, _, candidate, data, roots = values
        path = roots["package"] / "baseline.zip"
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", UserWarning)
            with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as archive:
                for entry in manifest["entries"]:
                    name = entry["path"]
                    payload = (roots["package"] / "extracted" / name).read_bytes()
                    archive.writestr(name, b"altered archive bytes" if variant == "tamper" and name.endswith("content.pak") else payload)
                if variant == "extra":
                    archive.writestr("extra.txt", "not in inventory")
                if variant == "traversal":
                    archive.writestr("../escape", "never extract")
                if variant == "alias":
                    archive.writestr(manifest["entries"][0]["path"].upper(), "alias")
                if variant == "symlink":
                    item = zipfile.ZipInfo("link")
                    item.create_system = 3
                    item.external_attr = (stat.S_IFLNK | 0o777) << 16
                    archive.writestr(item, "../outside")
        archive_ref = {"root": "package", "path": "baseline.zip", "sizeBytes": path.stat().st_size,
                       "sha256": ge.file_digest(path)[1]}
        candidate["packageSha256"] = archive_ref["sha256"]
        receipt["packageSha256"] = archive_ref["sha256"]
        receipt["packageIdentityKind"] = "archive-bytes-v1"
        receipt["extensions"] = {"archiveFormat": "zip-v1"}
        values[2] = {"kind": "archive-bytes-v1", "sha256": archive_ref["sha256"], "archive": archive_ref}
        return values

    def visual(self):
        from PIL import Image
        record = self.common("PlayableCandidateVisual")
        record["captures"] = []
        lanes = {"lanes": []}
        for n, lane in enumerate(ge.OLD_LANES):
            process = {"pid": 100 + n, "role": "client", "arguments": ["-d3d12"],
                       "startedAtUtc": record["startedAtUtc"], "completedAtUtc": record["completedAtUtc"]}
            lanes["lanes"].append({"laneId": lane, "execution": {"processes": [process]}})
            for state in sorted(br.VISUAL_STATES):
                capture = {"laneId": lane, "state": state, "status": "PASS"}
                if lane.startswith("Aura-") and state == "firearm":
                    capture.update(status="NotApplicable", reason="Aura has no firearm")
                else:
                    image = Image.new("RGB", (32, 24), (20 + n, 80, 140))
                    image.putpixel((0, 0), (len(record["captures"]), 1, 2))
                    buf = io.BytesIO()
                    image.save(buf, format="PNG")
                    capture.update(image=self.file(f"captures/{lane}/{state}.png", buf.getvalue()),
                                   resolution=[32, 24], quality="Medium", renderer="D3D12",
                                   capturedAtUtc="2026-08-31T00:05:00Z", processId=process["pid"])
                record["captures"].append(capture)
        return record, lanes

    def human(self):
        bundle = {key: self.file(key + ".json", json.dumps({"synthetic": key}).encode()) for key in ge.BUNDLE_REFS}
        record = self.common("GameplayBaselineHumanReview")
        record.update(implementerId="synthetic-author", reviewerId="synthetic-reviewer", operatorId="synthetic-operator",
                      implementerEnvironmentId="synthetic-env1", operatorEnvironmentId="synthetic-env2",
                      authorizedCollection=True, reviewedAtUtc="2026-08-31T00:09:00Z",
                      reviewedHashes={key: bundle[key]["sha256"] for key in set(ge.BUNDLE_REFS) - {
                          "humanReview", "legacyScope", "legacyContentManifest", "finalFast"}},
                      dispositions={key: "PASS" for key in br.REVIEW_DISPOSITIONS}, findings=[], blockingFindings=[])
        records = {"operations": {"operatorId": "synthetic-operator", "operatorEnvironmentId": "synthetic-env2",
                                  "completedAtUtc": "2026-08-31T00:08:00Z"}}
        return record, bundle, records

    def test_zip_exact_bytes_accept_and_track_transitive_inputs(self):
        values = self.archive()
        ge.validate_package(*values, preserved=self.preserved)
        self.assertIn(self.root / "package" / "baseline.zip", self.preserved)
        self.assertEqual(len(self.preserved), 6)

    def test_zip_tampered_extracted_bytes_rejected(self):
        values = self.archive()
        (self.root / "package/extracted/client/content.pak").write_bytes(b"different")
        self.fails("EVIDENCE_HASH_MISMATCH", lambda: ge.validate_package(*values))

    def test_zip_tampered_member_rejected(self):
        self.fails("EVIDENCE_HASH_MISMATCH", lambda: ge.validate_package(*self.archive("tamper")))

    def test_zip_extra_member_rejected(self):
        self.fails("PACKAGE_INVENTORY_INCOMPLETE", lambda: ge.validate_package(*self.archive("extra")))

    def test_zip_traversal_never_extracted(self):
        self.fails("UNSAFE_PATH", lambda: ge.validate_package(*self.archive("traversal")))
        self.assertFalse((self.root / "escape").exists())

    def test_zip_case_alias_rejected(self):
        self.fails("DUPLICATE_PATH", lambda: ge.validate_package(*self.archive("alias")))

    def test_zip_symlink_rejected(self):
        self.fails("UNSAFE_PATH", lambda: ge.validate_package(*self.archive("symlink")))

    def test_zip_unsupported_format_blocked(self):
        values = self.archive()
        values[1]["extensions"]["archiveFormat"] = "rar"
        with self.assertRaises(ge.EvidenceError) as caught:
            ge.validate_package(*values)
        self.assertEqual((caught.exception.code, caught.exception.status), ("UNSUPPORTED_ARCHIVE_FORMAT", "BLOCKED"))

    def test_zip_deadline_is_timeout(self):
        manifest, receipt, identity, _, _, roots = self.archive()
        with mock.patch.object(br.time, "monotonic", side_effect=[0, 121]):
            with self.assertRaises(ge.EvidenceError) as caught:
                br.archive_inventory(manifest, receipt, identity, br.References(roots, {}))
        self.assertEqual((caught.exception.code, caught.exception.status), ("ARCHIVE_TIMEOUT", "TIMEOUT"))

    def test_zip_corrupt_compressed_stream_is_typed_failure(self):
        values = self.archive()
        path = self.root / "package/baseline.zip"
        with zipfile.ZipFile(path) as archive:
            member = archive.infolist()[0]
            offset = member.header_offset + 30 + len(member.filename.encode()) + len(member.extra)
        data = bytearray(path.read_bytes())
        data[offset] ^= 255
        path.write_bytes(data)
        hashed = ge.digest(data)
        values[2]["archive"]["sha256"] = values[2]["sha256"] = values[3]["packageSha256"] = values[1]["packageSha256"] = hashed
        with self.assertRaises(ge.EvidenceError) as caught:
            ge.validate_package(*values)
        self.assertEqual(caught.exception.status, "FAIL")

    def test_zip_metadata_read_is_bounded_before_parser_allocation(self):
        raw = io.BytesIO(b"small file")
        reader = br.BoundedArchiveFile(raw)
        self.fails("ARCHIVE_LIMIT", lambda: reader.read(64 * 1024 * 1024 + 1))
        self.assertEqual(raw.tell(), 0)
        self.assertEqual(reader.read(), b"small file")

    def test_visual_duplicate_frame_reference_rejected(self):
        record, lanes = self.visual()
        record["captures"][1]["image"] = record["captures"][0]["image"]
        self.fails("DUPLICATE_PATH", lambda: br.validate_visual(record, self.candidate, lanes, self.refs))

    def test_visual_nullrhi_assignment_rejected(self):
        record, lanes = self.visual()
        lanes["lanes"][0]["execution"]["processes"][0]["arguments"] = ["-NullRHI=true"]
        self.fails("UNRENDERED_CAPTURE", lambda: br.validate_visual(record, self.candidate, lanes, self.refs))

    def test_rendered_four_lane_coverage_decodes(self):
        record, lanes = self.visual()
        br.validate_visual(record, self.candidate, lanes, self.refs)
        self.assertEqual(len(self.preserved), 28)  # 26 actual frames plus notes/producer

    def test_visual_nullrhi_process_rejected(self):
        record, lanes = self.visual()
        lanes["lanes"][0]["execution"]["processes"][0]["arguments"] = ["-NullRHI"]
        self.fails("UNRENDERED_CAPTURE", lambda: br.validate_visual(record, self.candidate, lanes, self.refs))

    def test_visual_server_capture_rejected(self):
        record, lanes = self.visual()
        lanes["lanes"][0]["execution"]["processes"][0]["role"] = "dedicated-server"
        self.fails("CAPTURE_PROVENANCE", lambda: br.validate_visual(record, self.candidate, lanes, self.refs))

    def test_visual_wrong_dimensions_rejected(self):
        record, lanes = self.visual()
        record["captures"][0]["resolution"] = [32, 25]
        self.fails("CAPTURE_DIMENSION_MISMATCH", lambda: br.validate_visual(record, self.candidate, lanes, self.refs))

    def test_visual_boolean_dimension_rejected(self):
        record, lanes = self.visual()
        record["captures"][0]["resolution"] = [True, 24]
        self.fails("INVALID_TYPE", lambda: br.validate_visual(record, self.candidate, lanes, self.refs))

    def test_visual_corrupt_image_rejected_despite_updated_hash(self):
        record, lanes = self.visual()
        record["captures"][0]["image"] = self.file("corrupt.png", b"\x89PNG\r\n\x1a\nnot pixels")
        self.fails("INVALID_IMAGE", lambda: br.validate_visual(record, self.candidate, lanes, self.refs))

    def test_visual_capture_outside_process_rejected(self):
        record, lanes = self.visual()
        lanes["lanes"][0]["execution"]["processes"][0]["completedAtUtc"] = "2026-08-31T00:04:00Z"
        self.fails("UNRENDERED_CAPTURE", lambda: br.validate_visual(record, self.candidate, lanes, self.refs))

    def test_visual_missing_state_rejected(self):
        record, lanes = self.visual()
        record["captures"].pop()
        self.fails("VISUAL_COVERAGE", lambda: br.validate_visual(record, self.candidate, lanes, self.refs))

    def test_visual_aura_notapplicable_needs_role_reason(self):
        record, lanes = self.visual()
        next(c for c in record["captures"] if c["status"] == "NotApplicable")["reason"] = "skipped"
        self.fails("VISUAL_COVERAGE", lambda: br.validate_visual(record, self.candidate, lanes, self.refs))

    def test_human_complete_attributed_record(self):
        record, bundle, records = self.human()
        br.validate_human(record, self.candidate, bundle, records, self.refs)

    def test_human_same_author_or_environment_rejected(self):
        for key, value in (("reviewerId", "synthetic-author"), ("operatorId", "synthetic-author"),
                           ("operatorEnvironmentId", "synthetic-env1")):
            record, bundle, records = self.human()
            record[key] = value
            self.fails("REVIEW_NOT_INDEPENDENT", lambda: br.validate_human(record, self.candidate, bundle, records, self.refs))

    def test_human_consent_boolean_not_coerced(self):
        record, bundle, records = self.human()
        record["authorizedCollection"] = "true"
        self.fails("HUMAN_REVIEW_MISSING", lambda: br.validate_human(record, self.candidate, bundle, records, self.refs))

    def test_human_record_tamper_invalidates_review(self):
        record, bundle, records = self.human()
        bundle["candidate"]["sha256"] = "a" * 64
        self.fails("REVIEW_COVERAGE", lambda: br.validate_human(record, self.candidate, bundle, records, self.refs))

    def test_human_unresolved_p1_not_hidden_by_empty_blocking_list(self):
        record, bundle, records = self.human()
        record["findings"] = [{"id": "issue1", "severity": "P1", "status": "Open", "contractBlocking": False, "summary": "synthetic defect"}]
        self.fails("HUMAN_REVIEW_BLOCKED", lambda: br.validate_human(record, self.candidate, bundle, records, self.refs))

    def test_human_resolved_finding_requires_real_evidence_bytes(self):
        record, bundle, records = self.human()
        record["findings"] = [{"id": "issue1", "severity": "P1", "status": "Resolved", "contractBlocking": False,
                                "summary": "synthetic defect", "resolutionEvidence": [self.evidence]}]
        br.validate_human(record, self.candidate, bundle, records, self.refs)
        (self.root / self.evidence["path"]).write_bytes(b"tampered")
        self.fails("EVIDENCE_HASH_MISMATCH", lambda: br.validate_human(record, self.candidate, bundle, records, self.refs))

    def test_human_review_cannot_predate_evidence(self):
        record, bundle, records = self.human()
        records["operations"]["completedAtUtc"] = "2026-08-31T00:09:30Z"
        self.fails("REVIEW_TIMESTAMP", lambda: br.validate_human(record, self.candidate, bundle, records, self.refs))

    def test_human_operator_binding_required(self):
        record, bundle, records = self.human()
        records["operations"]["operatorId"] = "someone-else"
        self.fails("REVIEW_NOT_INDEPENDENT", lambda: br.validate_human(record, self.candidate, bundle, records, self.refs))

    def test_unknown_receipt_producer_is_blocked(self):
        record, bundle, records = self.human()
        record["extensions"]["producer"]["producerVersion"] = 2
        with self.assertRaises(ge.EvidenceError) as caught:
            br.validate_human(record, self.candidate, bundle, records, self.refs)
        self.assertEqual(caught.exception.status, "BLOCKED")

    def test_reader_unknown_fields_rejected(self):
        record, bundle, records = self.human()
        record["trustMe"] = True
        self.fails("UNKNOWN_FIELD", lambda: br.validate_human(record, self.candidate, bundle, records, self.refs))

    def test_transitive_evidence_alias_hash_conflict_rejected(self):
        self.refs.read(self.evidence, "first")
        new_ref = self.file(self.evidence["path"], b"new bytes for same path")
        self.fails("BINDING_MISMATCH", lambda: self.refs.read(new_ref, "second"))

    def audit_bundle(self):
        """All implemented readers pass; unsupported runtime proof remains blocked."""
        from test_gameplay_expansion import write_json
        head, scope_path = self.fixture.fixture_repo()
        manifest, receipt, identity, candidate, _, roots = self.fixture.package_fixture()
        candidate["sourceRevision"] = manifest["sourceRevision"] = receipt["sourceRevision"] = head
        raw_manifest = json.dumps(manifest).encode()
        candidate["packageSha256"] = receipt["packageSha256"] = identity["sha256"] = ge.digest(raw_manifest)
        receipt["packageManifestSha256"] = ge.digest(raw_manifest)
        self.candidate = candidate
        for key, field in (("draft", "draftPath"), ("soak", "soakPath"), ("laneEvidence", "laneEvidencePath"), ("finalFast", "finalFastPath")):
            candidate[field] = str(self.root / (key + ".json"))
        binding = self.fixture.bound(candidate)
        visual, lanes = self.visual()
        for lane in lanes["lanes"]:
            role, topology = ge.OLD_LANES[lane["laneId"]]
            lane.update(role=role, topology=topology, status="PASS", packageSha256=candidate["packageSha256"], checkpoints=[{"synthetic": True}])
        lanes.update(binding, schemaVersion=2)
        soak = dict(binding, schemaVersion=2, status="PASS", passed=True, cyclesPerLane=10,
                    lanes=[{"laneId": lane, "cycles": [{"cycleId": n} for n in range(1, 11)]} for lane in ge.OLD_LANES])
        draft = dict(binding, schemaVersion=2, stage="Candidate", mode="Both", role="Both", status="PASS",
                     localDisposition="PASS", localContractStatus="PASS", packagedStatus="PASS", workingTreeStatus="", blockers=[])
        human, _, records = self.human()
        human["reviewedAtUtc"] = "2026-08-31T00:11:00Z"
        human["completedAtUtc"] = "2026-08-31T00:12:00Z"
        records.update(candidate=candidate, draft=draft, laneEvidence=lanes, soak=soak,
                       finalFast={"schemaVersion": 1, "scopeRevision": "playable-candidate-v1", "days": "all", "mode": "Both",
                                  "testsRun": 20, "failures": [], "passed": True, "durationSeconds": 1.0},
                       packageManifest=manifest, buildReceipt=receipt, visual=visual,
                       hardware={"synthetic": True}, baselineObservations=dict(binding), anchorSurvey=dict(binding))
        records["operations"].update(binding)
        bundle = {"schemaVersion": 1, "kind": "GameplayBaselineEvidence", "packageIdentity": identity}
        for key, record in records.items():
            data = raw_manifest if key == "packageManifest" else json.dumps(record).encode()
            bundle[key] = self.file(key + ".json", data)
        for key, path in (("legacyScope", ge.LEGACY_SCOPE), ("legacyContentManifest", ge.LEGACY_CONTENT)):
            bundle[key] = self.file(key + ".json", (self.fixture.repo / path).read_bytes())
        bundle["baselineTrace"] = self.file("baseline.utrace", b"SYNTHETIC - not a real trace")
        human["reviewedHashes"] = {key: bundle[key]["sha256"] for key in human["reviewedHashes"]}
        bundle["humanReview"] = self.file("humanReview.json", json.dumps(human).encode())
        write_json(self.root / "bundle.json", bundle)
        return scope_path, self.root / "bundle.json", roots, bundle

    def test_full_structural_bundle_passes_support_readers_but_not_runtime(self):
        scope_path, bundle_path, roots, _ = self.audit_bundle()
        result = ge.audit(self.fixture.repo, scope_path, bundle_path, roots["evidence"], roots["package"])
        failures = [check for check in result["checks"] if check["status"] == "FAIL"]
        self.assertEqual(failures, [])
        checks = {check["name"]: check for check in result["checks"]}
        for name in ("PackageInventory", "RenderedVisualReceipts", "IndependentHumanAttestation"):
            self.assertEqual(checks[name]["status"], "PASS", checks[name])
        self.assertEqual(result["runtimeEntry"], "BLOCKED_RUNTIME_ENTRY")
        self.assertFalse(result["capabilities"]["publicInputCheckpointSemantics"])

    def test_cli_reader_failure_is_typed_not_unhandled_exception(self):
        from test_gameplay_expansion import write_json
        scope_path, bundle_path, roots, bundle = self.audit_bundle()
        record = json.loads((self.root / "visual.json").read_text())
        record["captures"][0]["renderer"] = "NullRHI"
        bundle["visual"] = self.file("visual.json", json.dumps(record).encode())
        write_json(bundle_path, bundle)
        output = self.root / "reader-cli.json"
        result = subprocess.run([sys.executable, str(Path(ge.__file__)), "audit", "--repo-root", str(self.fixture.repo),
                                 "--scope", str(scope_path), "--baseline-evidence", str(bundle_path),
                                 "--evidence-root", str(roots["evidence"]), "--package-root", str(roots["package"]),
                                 "--output", str(output)], capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 1, result.stderr.decode())
        self.assertNotIn(b"Traceback", result.stderr)
        report = json.loads(output.read_text())
        self.assertIn("UNRENDERED_CAPTURE", [check["reasonCode"] for check in report["checks"]])

    def test_transitive_capture_mutation_during_audit_is_detected(self):
        scope_path, bundle_path, roots, _ = self.audit_bundle()
        original = br.validate_human
        def mutate_after_support(*args):
            original(*args)
            (self.root / "captures/Aura-listen/attack.png").write_bytes(b"changed after decoding")
        with mock.patch.object(br, "validate_human", side_effect=mutate_after_support):
            report = ge.audit(self.fixture.repo, scope_path, bundle_path, roots["evidence"], roots["package"])
        self.assertIn("LEGACY_ARTIFACT_CHANGED", [check["reasonCode"] for check in report["checks"]])


if __name__ == "__main__":
    unittest.main()
