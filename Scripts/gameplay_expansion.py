#!/usr/bin/env python3
"""Day 41 integrity diagnostics. No gameplay or candidate publication authority.

This intentionally implements only the advertised capabilities. Unsupported
execution/container/survey readers remain explicit runtime-entry blockers.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import time
from typing import Any, Callable

# Readers import the shared validators. Keep one EvidenceError type when this
# file is run as a CLI as well as when it is imported by the test harness.
if __name__ == "__main__":
    sys.modules.setdefault("gameplay_expansion", sys.modules[__name__])


SHA = re.compile(r"[0-9a-f]{64}\Z")
COMMIT = re.compile(r"[0-9a-f]{40}\Z")
RUN_ID = re.compile(r"[A-Za-z0-9][A-Za-z0-9._-]{0,95}\Z")
SCOPE_PATH = "Docs/Plans/Gameplay-Expansion-Implementation/gameplay-expansion-scope.json"
LEGACY_SCOPE = "Docs/Plans/Playable-Candidate-Implementation/playable-candidate-scope.json"
LEGACY_CONTENT = "Content/Config/PlayableCandidateManifest.json"
BINDING = ("runId", "sourceRevision", "scopeRevision", "scopeManifestSha256",
           "contentManifestSha256", "packageSha256")
BUNDLE_REFS = ("candidate", "draft", "laneEvidence", "soak", "finalFast", "legacyScope",
               "legacyContentManifest", "packageManifest", "buildReceipt", "operations",
               "visual", "humanReview", "hardware", "baselineObservations", "baselineTrace",
               "anchorSurvey")
MEASURED_REFS = ("hardware", "baselineObservations", "baselineTrace", "anchorSurvey")
LANES = {
    "S-A": ("Standalone", ["Aura"]), "S-B": ("Standalone", ["BungeeMan"]),
    "L-AA": ("Listen", ["Aura", "Aura"]), "L-BB": ("Listen", ["BungeeMan", "BungeeMan"]),
    "L-AB": ("Listen", ["Aura", "BungeeMan"]), "L-BA": ("Listen", ["BungeeMan", "Aura"]),
    "D-AA": ("Dedicated", ["Aura", "Aura"]), "D-BB": ("Dedicated", ["BungeeMan", "BungeeMan"]),
    "D-AB": ("Dedicated", ["Aura", "BungeeMan"]),
}
OLD_LANES = {f"{role}-{topology}": (role, topology)
             for role in ("Aura", "BungeeMan") for topology in ("listen", "dedicated")}
CAPABILITIES = {
    "strictJsonAndScope": True, "pathContainmentAndFileHashes": True,
    "gitAncestryAndLegacyBlobs": True, "candidateAndRecordBindings": True,
    "packageInventoryBytes": True, "freshNativeReport": True,
    "buildReceiptProvenance": False, "cookedContainerContents": False,
    "archiveInventory": True, "publicInputCheckpointSemantics": False,
    "soakMetricsAndIsolation": False, "shippingOperationsAndIdentity": False,
    "renderedVisualEvidence": True, "independentHumanAttestations": True,
    "engineAnchorSurvey": False,
}


class EvidenceError(Exception):
    def __init__(self, code: str, message: str, status: str = "FAIL"):
        super().__init__(message)
        self.code, self.status = code, status


def require(condition: bool, code: str, message: str) -> None:
    if not condition:
        raise EvidenceError(code, message)


def obj(value: Any, name: str, required: set[str] | None = None,
        allowed: set[str] | None = None) -> dict:
    require(type(value) is dict, "INVALID_TYPE", f"{name} must be an object")
    if required is not None:
        require(required <= value.keys(), "MISSING_FIELD", f"{name} missing: {sorted(required - value.keys())}")
    if allowed is not None:
        require(value.keys() <= allowed, "UNKNOWN_FIELD", f"{name} unknown: {sorted(value.keys() - allowed)}")
        if "extensions" in allowed and "extensions" in value:
            require(type(value["extensions"]) is dict, "INVALID_TYPE", f"{name}.extensions must be an object")
    return value


def integer(value: Any, name: str, minimum: int = 0) -> int:
    require(type(value) is int and value >= minimum, "INVALID_TYPE", f"{name} must be an integer >= {minimum}")
    return value


def number(value: Any, name: str, minimum: float = 0.0) -> float:
    require(type(value) in (int, float) and math.isfinite(value) and value >= minimum,
            "INVALID_TYPE", f"{name} must be a finite number >= {minimum}")
    return value


def string(value: Any, name: str) -> str:
    require(type(value) is str and bool(value.strip()), "INVALID_TYPE", f"{name} must be a nonempty string")
    return value


def sha(value: Any, name: str) -> str:
    require(type(value) is str and SHA.fullmatch(value) is not None, "INVALID_HASH", f"{name} must be lowercase SHA-256")
    return value


def utc(value: Any, name: str) -> datetime:
    string(value, name)
    # .NET round-trip timestamps carry 100ns ticks; Python 3.10 accepts at most
    # six fractional digits. Discard only that final sub-microsecond digit,
    # leaving date and offset validation to datetime and the UTC check below.
    value = re.sub(r"\A(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{6})\d(Z|[+-]\d{2}:\d{2})\Z",
                   r"\1\2", value)
    try:
        result = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as exc:
        raise EvidenceError("INVALID_TIMESTAMP", f"{name} is not ISO-8601") from exc
    require(result.utcoffset() is not None and result.utcoffset().total_seconds() == 0,
            "INVALID_TIMESTAMP", f"{name} must explicitly use UTC")
    return result


def validate_run_id(value: Any) -> str:
    require(type(value) is str and RUN_ID.fullmatch(value) is not None and value not in (".", ".."),
            "UNSAFE_RUN_ID", "runId contains unsupported characters or length")
    require(not value.endswith(".") and re.fullmatch(r"(?i)(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\..*)?", value) is None,
            "UNSAFE_RUN_ID", "runId is a reserved Windows name or has a trailing dot")
    return value


def strict_json(data: bytes, label: str) -> dict:
    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, "DUPLICATE_JSON_KEY", f"{label}: duplicate key {key!r}")
            result[key] = value
        return result
    def constant(value):
        raise EvidenceError("NONFINITE_JSON", f"{label}: non-finite value {value}")
    try:
        # Windows PowerShell 5 writes UTF-8 BOMs in existing evidence files.
        value = json.loads(data.decode("utf-8-sig"), object_pairs_hook=pairs, parse_constant=constant)
    except (ValueError, UnicodeError) as exc:
        raise EvidenceError("INVALID_JSON", f"{label}: {exc}") from exc
    def finite(item):
        if type(item) is float:
            require(math.isfinite(item), "NONFINITE_JSON", f"{label}: non-finite numeric value")
        elif type(item) is dict:
            for child in item.values():
                finite(child)
        elif type(item) is list:
            for child in item:
                finite(child)
    finite(value)
    return obj(value, label)


def reject_reparse(path: Path) -> None:
    for component in (path, *path.parents):
        try:
            info = component.lstat()
        except FileNotFoundError:
            continue
        if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
            raise EvidenceError("UNSAFE_PATH", f"reparse/symlink component: {component}")


def root_path(path: Path) -> Path:
    path = Path(os.path.abspath(path))
    reject_reparse(path)
    return path


def relative_path(value: Any) -> Path:
    string(value, "relative path")
    require(not any(c in value for c in ':*?\x00<>|"') and not value.startswith(("/", "\\")),
            "UNSAFE_PATH", f"unsafe relative path: {value!r}")
    parts = value.replace("\\", "/").split("/")
    require(all(p not in ("", ".", "..") and p == p.strip() and not p.endswith(".") for p in parts),
            "UNSAFE_PATH", f"unsafe path component: {value!r}")
    require(all(not re.fullmatch(r"(?i)(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\..*)?", p) for p in parts),
            "UNSAFE_PATH", f"reserved device name: {value!r}")
    return Path(*parts)


def contained(root: Path, path: Path) -> Path:
    root, path = root_path(root), root_path(path)
    require(path != root and root in path.parents, "UNSAFE_PATH", f"path is outside declared root: {path}")
    return path


def file_bytes(path: Path) -> bytes:
    reject_reparse(path)
    try:
        before = path.stat()
        require(stat.S_ISREG(before.st_mode), "INVALID_FILE", f"not a regular file: {path}")
        require(before.st_size <= 64 * 1024 * 1024, "RECORD_TOO_LARGE", f"record/log exceeds 64 MiB parser limit: {path}")
        with path.open("rb") as handle:
            opened = os.fstat(handle.fileno())
            data = handle.read()
            after_open = os.fstat(handle.fileno())
        after = path.stat()
    except FileNotFoundError as exc:
        raise EvidenceError("EVIDENCE_MISSING", f"missing file: {path}", "BLOCKED") from exc
    fingerprint = lambda info: (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns)
    require(fingerprint(before) == fingerprint(opened) == fingerprint(after_open) == fingerprint(after),
            "FILE_CHANGED_DURING_READ", f"file changed while reading: {path}")
    return data


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def file_digest(path: Path) -> tuple[int, str]:
    """Stream package/trace bytes without loading multi-gigabyte files into RAM."""
    reject_reparse(path)
    try:
        before = path.stat()
        require(stat.S_ISREG(before.st_mode), "INVALID_FILE", f"not a regular file: {path}")
        hasher = hashlib.sha256()
        with path.open("rb") as handle:
            opened = os.fstat(handle.fileno())
            for chunk in iter(lambda: handle.read(1024 * 1024), b""):
                hasher.update(chunk)
            after_open = os.fstat(handle.fileno())
        after = path.stat()
    except FileNotFoundError as exc:
        raise EvidenceError("EVIDENCE_MISSING", f"missing file: {path}", "BLOCKED") from exc
    fingerprint = lambda info: (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns)
    require(fingerprint(before) == fingerprint(opened) == fingerprint(after_open) == fingerprint(after),
            "FILE_CHANGED_DURING_READ", f"file changed while hashing: {path}")
    return after.st_size, hasher.hexdigest()


def read_ref(ref: Any, roots: dict[str, Path | None], name: str, read_data: bool = True) -> tuple[Path, bytes | None]:
    fields = {"root", "path", "sha256", "sizeBytes"}
    ref = obj(ref, name, fields, fields)
    require(ref["root"] in ("evidence", "package"), "UNSAFE_PATH", f"{name}: unknown root")
    relative = relative_path(ref["path"])
    sha(ref["sha256"], f"{name}.sha256")
    integer(ref["sizeBytes"], f"{name}.sizeBytes", 1)
    root = roots.get(ref["root"])
    if root is None:
        raise EvidenceError("PACKAGE_ROOT_MISSING", "explicit package root is required", "BLOCKED")
    path = contained(root, root / relative)
    size, actual_hash = file_digest(path)
    require(size == ref["sizeBytes"] and actual_hash == ref["sha256"],
            "EVIDENCE_HASH_MISMATCH", f"{name}: size or SHA-256 mismatch")
    data = file_bytes(path) if read_data else None
    if data is not None:
        require(digest(data) == actual_hash, "FILE_CHANGED_DURING_READ", f"{name}: changed between hash and parse")
    return path, data


def git(repo: Path, *args: str, ancestry: bool = False) -> bytes:
    try:
        result = subprocess.run(["git", "-C", str(repo), *args], capture_output=True, timeout=20, check=False)
    except subprocess.TimeoutExpired as exc:
        raise EvidenceError("GIT_TIMEOUT", "Git command exceeded the 20-second process budget", "TIMEOUT") from exc
    except OSError as exc:
        raise EvidenceError("GIT_FAILED", f"Git command failed: {exc}") from exc
    if ancestry and result.returncode == 1:
        raise EvidenceError("SOURCE_NOT_ANCESTOR", "baseline source is not an ancestor of audit HEAD")
    require(result.returncode == 0, "GIT_FAILED", f"git {' '.join(args)} failed with exit {result.returncode}")
    return result.stdout


def commit_ancestor(repo: Path, source: Any, head: str) -> None:
    require(type(source) is str and COMMIT.fullmatch(source) is not None, "INVALID_REVISION", "full source commit ID required")
    resolved = git(repo, "rev-parse", "--verify", f"{source}^{{commit}}").decode().strip()
    require(resolved == source, "INVALID_REVISION", "source does not resolve exactly")
    git(repo, "merge-base", "--is-ancestor", source, head, ancestry=True)


def validate_scope(scope: dict) -> None:
    fields = {"schemaVersion", "kind", "scopeRevision", "phase", "gameplayProfile", "canonicalMap", "mapAlias",
              "sourceRevisionAtFreeze", "legacyScope", "legacyContentManifest", "features", "lanes", "seeds",
              "authority", "budgets", "baselineEvidence", "hardware", "baselineObservations", "baselineTrace",
              "anchorSurvey", "blockers"}
    obj(scope, "scope", fields, fields | {"extensions"})
    require(type(scope["schemaVersion"]) is int and scope["schemaVersion"] == 1, "INVALID_SCHEMA", "scope schemaVersion must be integer 1")
    require(scope["kind"] == "GameplayExpansionScope" and scope["gameplayProfile"] == "GameplayExpansionV1",
            "INVALID_SCOPE", "scope kind/profile mismatch")
    require(scope["phase"] in ("Draft", "Frozen"), "INVALID_PHASE", "phase must be Draft or Frozen")
    string(scope["scopeRevision"], "scopeRevision")
    require(scope["canonicalMap"] == "/Game/Maps/StartupMap" and scope["mapAlias"] == "RoleBattleCivilianTest",
            "INVALID_SCOPE", "canonical map/alias mismatch")
    for key, expected in (("legacyScope", LEGACY_SCOPE), ("legacyContentManifest", LEGACY_CONTENT)):
        ref = obj(scope[key], key, {"path", "sha256"}, {"path", "sha256"})
        require(ref["path"] == expected, "INVALID_SCOPE", f"{key} must reference unchanged legacy manifest")
        sha(ref["sha256"], key)
    expected_features = {
        "roles": ["Aura", "BungeeMan"], "missionTemplates": ["Assault", "RescueRelay", "Sabotage"],
        "enemyArchetypes": ["Raider", "Lancer", "Bulwark", "Disruptor"],
        "augments": ["quick_step", "guarded_step", "steady_hands", "field_medic", "forked_spark", "ember_ring", "fast_cycle", "wide_trap"],
        "mutators": ["RestlessPatrols", "VolatileVents"],
    }
    feature_numbers = {"arenaArrangements": 2, "combatCellsPerRun": 2, "augmentChoicesPerRun": 2,
                       "offersPerChoice": 3, "captainBosses": 1, "masteryBadges": 3}
    feature_fields = set(expected_features) | set(feature_numbers)
    features = obj(scope["features"], "features", feature_fields, feature_fields)
    for key, value in expected_features.items():
        require(features.get(key) == value, "FEATURE_INVENTORY_MISMATCH", f"features.{key} differs from approved inventory")
    for key, value in feature_numbers.items():
        require(type(features.get(key)) is int and features[key] == value, "FEATURE_INVENTORY_MISMATCH", f"features.{key} mismatch")
    require(type(scope["lanes"]) is list and len(scope["lanes"]) == len(LANES), "LANE_MISMATCH", "exactly nine gameplay lanes required")
    seen = set()
    for lane in scope["lanes"]:
        lane = obj(lane, "lane", {"id", "topology", "roles"}, {"id", "topology", "roles"})
        lane_id = string(lane["id"], "lane.id")
        require(lane_id in LANES and lane_id not in seen, "LANE_MISMATCH", "unknown or duplicate gameplay lane")
        seen.add(lane_id)
        require((lane["topology"], lane["roles"]) == LANES[lane_id], "LANE_MISMATCH", f"wrong topology/role order for {lane_id}")
    seeds = obj(scope["seeds"], "seeds", {"Core", "Fault"}, {"Core", "Fault"})
    require(type(seeds["Core"]) is list and all(type(x) is int for x in seeds["Core"]) and type(seeds["Fault"]) is int and
            seeds == {"Core": [41001, 41002, 41003], "Fault": 41999}, "SEED_MISMATCH", "declared seed matrix changed")
    authority_fields = {"profileSelection": "server-only", "rewardSettlement": "server-exactly-once",
                        "runState": "transient-authority", "persistentProfile": "hub-snapshot-isolated",
                        "missionAi": "single-UE-behavior-tree-owner"}
    auth = obj(scope["authority"], "authority", set(authority_fields), set(authority_fields))
    for key, expected in authority_fields.items():
        require(auth.get(key) == expected, "AUTHORITY_CONTRACT_MISSING", f"authority.{key} missing/mismatched")
    targets = {"serverHz": 30, "serverGameThreadP95Ms": 25, "serverGameThreadP99Ms": 33.3,
               "clientFrameP95Ms": 22, "clientFrameP99Ms": 33.3, "schedulingP95Ms": 2,
               "outboundP95KiBps": 100, "outboundGrowthLimitPercent": 25, "memoryGrowthLimitPercent": 5,
               "warmupSeconds": 300, "sampleSeconds": 600, "sampleRepeats": 3,
               "maxHostilesSolo": 10, "maxHostilesCoop": 16, "runCapSeconds": 1200}
    budget_fields = set(targets) | {"classification", "resolution", "quality"}
    budgets = obj(scope["budgets"], "budgets", budget_fields, budget_fields)
    for key, expected in targets.items():
        require(number(budgets.get(key), key) == expected, "BUDGET_MISMATCH", f"budgets.{key} differs from approved target")
    require(type(budgets.get("resolution")) is list and all(type(x) is int for x in budgets["resolution"]) and
            budgets.get("classification") == "acceptance-targets-not-measurements" and
            budgets.get("resolution") == [1920, 1080] and budgets.get("quality") == "Medium",
            "BUDGET_MISMATCH", "target settings/classification required")
    require(type(scope["blockers"]) is list, "INVALID_TYPE", "blockers must be an array")
    for blocker in scope["blockers"]:
        obj(blocker, "blocker", {"reasonCode", "description"}, {"reasonCode", "description"})
        string(blocker["reasonCode"], "blocker.reasonCode")
        string(blocker["description"], "blocker.description")
    if "extensions" in scope:
        obj(scope["extensions"], "scope.extensions")
    if scope["phase"] == "Draft":
        require(scope["sourceRevisionAtFreeze"] is None, "INVALID_PHASE", "Draft cannot claim source-at-freeze")
        require(bool(scope["blockers"]), "MISSING_BLOCKER", "Draft must list typed blockers")
    else:
        require(type(scope["sourceRevisionAtFreeze"]) is str and COMMIT.fullmatch(scope["sourceRevisionAtFreeze"]) is not None,
                "INVALID_REVISION", "Frozen requires source-at-freeze")
        require(not scope["blockers"], "INVALID_PHASE", "Frozen cannot retain declared blockers")
        require(all(scope[key] is not None for key in ("baselineEvidence", *MEASURED_REFS)),
                "FROZEN_EVIDENCE_MISSING", "Frozen requires measured baseline references")
    if scope["baselineEvidence"] is not None:
        reference = obj(scope["baselineEvidence"], "baselineEvidence", {"path", "sha256"}, {"path", "sha256"})
        relative_path(reference["path"])
        sha(reference["sha256"], "baselineEvidence.sha256")
    for key in MEASURED_REFS:
        if scope[key] is not None:
            ref = obj(scope[key], key, {"root", "path", "sha256", "sizeBytes"}, {"root", "path", "sha256", "sizeBytes"})
            require(ref["root"] == "evidence", "UNSAFE_PATH", f"{key} must be an evidence reference")
            relative_path(ref["path"])
            sha(ref["sha256"], key)
            integer(ref["sizeBytes"], key, 1)


def validate_candidate(candidate: dict) -> None:
    fields = set(BINDING) | {"schemaVersion", "status", "localDisposition", "externalDisposition", "immutable",
                             "draftPath", "soakPath", "laneEvidencePath", "finalFastPath", "finalizedAtUtc"}
    if candidate.get("schemaVersion") == 1 or candidate.get("validationMode") == "local-contract":
        raise EvidenceError("LEGACY_STUB_REJECTED", "local-contract candidate stubs are not Day 40 completion evidence")
    obj(candidate, "candidate", fields)
    require(type(candidate["schemaVersion"]) is int and candidate["schemaVersion"] == 2,
            "INVALID_SCHEMA", "candidate schemaVersion must be integer 2")
    require(candidate["status"] == candidate["localDisposition"] == "PASS" and candidate["immutable"] is True,
            "CANDIDATE_NOT_FINAL", "candidate must be immutable local PASS")
    require(candidate["externalDisposition"] in ("PASS", "BLOCKED"), "INVALID_DISPOSITION", "invalid external disposition")
    require(candidate["scopeRevision"] == "playable-candidate-v1", "BINDING_MISMATCH", "legacy scope revision differs")
    validate_run_id(candidate["runId"])
    require(type(candidate["sourceRevision"]) is str and COMMIT.fullmatch(candidate["sourceRevision"]) is not None,
            "INVALID_REVISION", "candidate needs full source revision")
    for key in ("scopeManifestSha256", "contentManifestSha256", "packageSha256"):
        sha(candidate[key], key)
    for key in ("draftPath", "soakPath", "laneEvidencePath", "finalFastPath"):
        string(candidate[key], key)
    utc(candidate["finalizedAtUtc"], "finalizedAtUtc")


def validate_binding(record: dict, candidate: dict, name: str) -> None:
    obj(record, name, set(BINDING))
    for key in BINDING:
        require(type(record[key]) is str and record[key] == candidate[key], "BINDING_MISMATCH", f"{name}.{key} mismatch")


def validate_draft(record: dict, candidate: dict) -> None:
    validate_binding(record, candidate, "draft")
    for key, expected in {"schemaVersion": 2, "stage": "Candidate", "mode": "Both", "role": "Both", "status": "PASS",
                          "localDisposition": "PASS", "localContractStatus": "PASS", "packagedStatus": "PASS",
                          "workingTreeStatus": "", "blockers": []}.items():
        require(type(record.get(key)) is type(expected) and record[key] == expected,
                "DRAFT_NOT_PACKAGED_PASS", f"draft.{key} does not qualify")


def validate_lanes(record: dict, candidate: dict, soak: bool = False) -> None:
    validate_binding(record, candidate, "soak" if soak else "laneEvidence")
    require(type(record.get("schemaVersion")) is int and record["schemaVersion"] == 2, "INVALID_SCHEMA", "lane/soak schema must be 2")
    lanes = record.get("lanes")
    require(type(lanes) is list and len(lanes) == 4, "LANE_MISMATCH", "four legacy lane records required")
    seen = set()
    if soak:
        require(record.get("status") == "PASS" and record.get("passed") is True and
                type(record.get("cyclesPerLane")) is int and record["cyclesPerLane"] == 10,
                "SOAK_CYCLES_INCOMPLETE", "soak must declare ten passing cycles")
    for lane in lanes:
        obj(lane, "legacy lane", {"laneId"})
        lane_id = string(lane["laneId"], "laneId")
        require(lane_id in OLD_LANES and lane_id not in seen, "LANE_MISMATCH", "duplicate/unknown legacy lane")
        seen.add(lane_id)
        if soak:
            cycles = lane.get("cycles")
            require(type(cycles) is list and len(cycles) == 10, "SOAK_CYCLES_INCOMPLETE", "aggregate cycle count is insufficient")
            ids = [obj(c, "cycle").get("cycleId") for c in cycles]
            require(all(type(x) is int for x in ids) and sorted(ids) == list(range(1, 11)),
                    "SOAK_CYCLES_INCOMPLETE", "cycles must be uniquely numbered 1 through 10")
        else:
            require((lane.get("role"), lane.get("topology")) == OLD_LANES[lane_id], "LANE_MISMATCH", "legacy role/topology mismatch")
            require(lane.get("status") == "PASS" and lane.get("packageSha256") == candidate["packageSha256"],
                    "BINDING_MISMATCH", "lane disposition/package mismatch")
            require(type(lane.get("execution")) is dict and type(lane.get("checkpoints")) is list and bool(lane["checkpoints"]),
                    "CHECKPOINT_EVIDENCE_INCOMPLETE", "lane needs execution and checkpoint receipts")


def validate_final_fast(record: dict) -> None:
    for key, expected in {"schemaVersion": 1, "scopeRevision": "playable-candidate-v1", "days": "all", "mode": "Both",
                          "failures": [], "passed": True}.items():
        require(type(record.get(key)) is type(expected) and record[key] == expected,
                "LEGACY_FAST_INVALID", f"finalFast.{key} mismatch")
    integer(record.get("testsRun"), "testsRun", 1)
    number(record.get("durationSeconds"), "durationSeconds")


def validate_package(manifest: dict, receipt: dict, identity: dict, candidate: dict,
                     manifest_data: bytes, roots: dict, preserved: dict | None = None) -> None:
    fields = {"schemaVersion", "kind", "sourceRevision", "runId", "scopeRevision", "scopeManifestSha256",
              "contentManifestSha256", "engineVersion", "createdAtUtc", "entries"}
    obj(manifest, "packageManifest", fields, fields | {"extensions"})
    require(type(manifest["schemaVersion"]) is int and manifest["schemaVersion"] == 1 and
            manifest["kind"] == "PlayableCandidatePackageInventory", "INVALID_SCHEMA", "unsupported package manifest schema")
    for key in BINDING[:-1]:
        require(manifest[key] == candidate[key], "BINDING_MISMATCH", f"packageManifest.{key} mismatch")
    string(manifest["engineVersion"], "engineVersion")
    utc(manifest["createdAtUtc"], "createdAtUtc")
    validate_binding(receipt, candidate, "buildReceipt")
    for key, expected in {"schemaVersion": 1, "kind": "PlayableCandidateBuildReceipt", "workingTreeStatusBefore": "",
                          "workingTreeStatusAfter": ""}.items():
        require(type(receipt.get(key)) is type(expected) and receipt[key] == expected,
                "DIRTY_PACKAGE_SOURCE" if key.startswith("workingTree") else "INVALID_SCHEMA", f"buildReceipt.{key} mismatch")
    require(receipt.get("packageManifestSha256") == digest(manifest_data), "BINDING_MISMATCH", "build receipt manifest hash mismatch")
    obj(identity, "packageIdentity", {"kind", "sha256", "archive"}, {"kind", "sha256", "archive"})
    sha(identity["sha256"], "packageIdentity.sha256")
    require(identity["sha256"] == candidate["packageSha256"] and receipt.get("packageIdentityKind") == identity["kind"],
            "PACKAGE_IDENTITY_UNPROVEN", "package identity is not bound by receipt")
    if identity["kind"] == "manifest-bytes-v1":
        require(identity["archive"] is None and identity["sha256"] == digest(manifest_data),
                "PACKAGE_IDENTITY_UNPROVEN", "manifest identity does not match actual manifest bytes")
    elif identity["kind"] == "archive-bytes-v1":
        read_ref(identity["archive"], roots, "package archive", read_data=False)
        require(identity["archive"]["sha256"] == identity["sha256"], "EVIDENCE_HASH_MISMATCH", "archive identity mismatch")
    else:
        raise EvidenceError("PACKAGE_IDENTITY_UNPROVEN", "unknown original package identity convention")
    if roots["package"] is None:
        raise EvidenceError("PACKAGE_ROOT_MISSING", "explicit package root required", "BLOCKED")
    subtree = contained(roots["package"], roots["package"] / relative_path(receipt.get("packageSubdirectory")))
    if not subtree.is_dir():
        raise EvidenceError("PACKAGE_BYTES_MISSING", "extracted package subtree is missing", "BLOCKED")
    entries = manifest["entries"]
    require(type(entries) is list and bool(entries), "PACKAGE_INVENTORY_INCOMPLETE", "package inventory is empty")
    seen, categories, ordering = set(), set(), []
    for entry in entries:
        fields = {"path", "sizeBytes", "sha256", "kind", "target", "configuration"}
        obj(entry, "package entry", fields, fields)
        relative = relative_path(entry["path"])
        normalized = relative.as_posix().casefold()
        require(normalized not in seen, "DUPLICATE_PATH", "duplicate package path")
        seen.add(normalized)
        ordering.append(normalized)
        integer(entry["sizeBytes"], "entry.sizeBytes")
        sha(entry["sha256"], "entry.sha256")
        require(entry["kind"] in ("launcher", "executable", "library", "container", "content", "runtime-resource") and
                entry["target"] in ("Aura", "AuraServer", "Shared") and entry["configuration"] in ("Development", "Shipping", "Shared"),
                "INVALID_SCHEMA", "invalid package category/target/configuration")
        categories.add((entry["target"], entry["kind"]))
        entry_path = contained(subtree, subtree / relative)
        size, actual_hash = file_digest(entry_path)
        require(size == entry["sizeBytes"] and actual_hash == entry["sha256"], "EVIDENCE_HASH_MISMATCH", f"package bytes changed: {relative}")
        if preserved is not None:
            preserved[entry_path] = actual_hash
    require(ordering == sorted(ordering), "INVALID_ORDER", "package entries must be sorted by normalized path")
    actual = set()
    for directory, dirs, files in os.walk(subtree, followlinks=False):
        for name in dirs + files:
            reject_reparse(Path(directory) / name)
        for name in files:
            actual.add((Path(directory) / name).relative_to(subtree).as_posix().casefold())
    require(actual == seen, "PACKAGE_INVENTORY_INCOMPLETE", "inventory does not enumerate the package bytes exactly")
    require({(target, kind) for target in ("Aura", "AuraServer") for kind in ("launcher", "executable")} <= categories,
            "PACKAGE_INVENTORY_INCOMPLETE", "both targets need launcher and inner executable")
    require(any(kind == "container" for _, kind in categories), "PACKAGE_INVENTORY_INCOMPLETE", "cooked container inventory missing")
    from gameplay_baseline_readers import References, archive_inventory
    archive_inventory(manifest, receipt, identity, References(roots, preserved if preserved is not None else {}))


class Report:
    def __init__(self, command: str):
        self.started = time.monotonic()
        self.data = {"schemaVersion": 1, "command": command, "startedAtUtc": datetime.now(timezone.utc).isoformat(),
                     "toolingStatus": "NOT_RUN", "checks": [], "capabilities": dict(CAPABILITIES),
                     "runtimeEntry": "BLOCKED_RUNTIME_ENTRY", "inputHashes": {}, "inputPaths": {}, "roots": {}, "phase": None}

    def check(self, name: str, action: Callable[[], Any]) -> Any:
        try:
            value = action()
            self.data["checks"].append({"name": name, "status": "PASS", "reasonCode": "VERIFIED"})
            return value
        except EvidenceError as exc:
            self.data["checks"].append({"name": name, "status": exc.status, "reasonCode": exc.code, "message": str(exc)})
        except (OSError, ValueError, TypeError, KeyError, RecursionError) as exc:
            self.data["checks"].append({"name": name, "status": "FAIL", "reasonCode": "AUDIT_ERROR", "message": str(exc)})
        return None

    def block(self, name: str, code: str, message: str) -> None:
        self.data["checks"].append({"name": name, "status": "BLOCKED", "reasonCode": code, "message": message})

    def finish(self) -> dict:
        terminal = [c for c in self.data["checks"] if c["status"] in ("TIMEOUT", "CLEANUP_FAILURE")]
        failures = [c for c in self.data["checks"] if c["status"] == "FAIL"]
        blocks = [c for c in self.data["checks"] if c["status"] == "BLOCKED"]
        self.data["status"] = terminal[0]["status"] if terminal else "FAIL" if failures else "BLOCKED" if blocks else "PASS"
        self.data["evidenceStatus"] = self.data["status"]
        self.data["reasonCode"] = (terminal or failures or blocks or [{"reasonCode": "VERIFIED"}])[0]["reasonCode"]
        self.data["completedAtUtc"] = datetime.now(timezone.utc).isoformat()
        self.data["durationSeconds"] = time.monotonic() - self.started
        # There is intentionally no code path granting runtimeEntry READY.
        return self.data


def audit(repo_root: Path, scope_path: Path, baseline_evidence: Path | None = None,
          evidence_root: Path | None = None, package_root: Path | None = None) -> dict:
    report = Report("audit")
    repo = root_path(repo_root)
    scope_file = contained(repo, scope_path if scope_path.is_absolute() else repo / scope_path)
    report.data["roots"] = {"repo": str(repo), "evidence": None, "package": None}
    report.data["inputPaths"]["scope"] = str(scope_file)
    scope_data = report.check("ScopeRead", lambda: file_bytes(scope_file))
    scope = report.check("ScopeJson", lambda: strict_json(scope_data, "scope")) if scope_data is not None else None
    scope_valid = False
    if scope is not None:
        previous = len(report.data["checks"])
        report.check("ScopeRequiredFields", lambda: validate_scope(scope))
        scope_valid = report.data["checks"][previous]["status"] == "PASS"
        report.data["phase"] = scope.get("phase")
        report.data["inputHashes"]["scope"] = digest(scope_data)
    head_bytes = report.check("AuditHead", lambda: git(repo, "rev-parse", "HEAD"))
    head = head_bytes.decode().strip() if head_bytes is not None else None
    report.data["sourceRevision"] = head
    dirty = report.check("WorkingTree", lambda: git(repo, "status", "--porcelain=v1", "--untracked-files=all"))
    report.data["workingTreeDirty"] = bool(dirty) if dirty is not None else None
    report.data["dirtyPaths"] = dirty.decode("utf-8", errors="replace").splitlines() if dirty is not None else []
    preserved = {}
    for key, path in (("legacyScope", LEGACY_SCOPE), ("legacyContentManifest", LEGACY_CONTENT)):
        report.data["inputPaths"][key] = str(repo / path)
        data = report.check(f"Read_{key}", lambda p=path: file_bytes(repo / p))
        if data is not None:
            preserved[repo / path] = digest(data)
            report.data["inputHashes"][key] = digest(data)
            if scope_valid:
                report.check(f"Unchanged_{key}", lambda k=key, d=data: require(digest(d) == scope[k]["sha256"], "LEGACY_MANIFEST_CHANGED", f"{k} changed from scope"))
    if scope_valid and scope["phase"] == "Draft":
        report.block("DraftNeverReady", "DRAFT_SCOPE", "Draft tooling does not authorize runtime implementation")
    elif scope_valid and head:
        report.check("FreezeSourceAncestry", lambda: commit_ancestor(repo, scope["sourceRevisionAtFreeze"], head))
    if baseline_evidence is None and scope_valid and scope["baselineEvidence"] is not None:
        baseline_evidence = contained(repo, repo / relative_path(scope["baselineEvidence"]["path"]))
    if baseline_evidence is None:
        report.block("BaselineEvidence", "BASELINE_NOT_SUPPLIED", "No explicit Day 40 baseline evidence bundle supplied")
    else:
        baseline_evidence = root_path(baseline_evidence)
        evidence = root_path(evidence_root or baseline_evidence.parent)
        roots = {"evidence": evidence, "package": root_path(package_root) if package_root else None}
        report.data["roots"].update({key: str(value) if value is not None else None for key, value in roots.items()})
        report.data["inputPaths"]["bundle"] = str(baseline_evidence)
        bundle_data = report.check("BundleRead", lambda: file_bytes(contained(evidence, baseline_evidence)))
        bundle = report.check("BundleJson", lambda: strict_json(bundle_data, "bundle")) if bundle_data is not None else None
        if bundle_data is not None:
            preserved[baseline_evidence] = digest(bundle_data)
            report.data["inputHashes"]["bundle"] = digest(bundle_data)
        if bundle is not None:
            def shape():
                obj(bundle, "bundle", {"schemaVersion", "kind", "packageIdentity", *BUNDLE_REFS}, {"schemaVersion", "kind", "packageIdentity", *BUNDLE_REFS, "extensions"})
                require(type(bundle["schemaVersion"]) is int and bundle["schemaVersion"] == 1 and bundle["kind"] == "GameplayBaselineEvidence", "INVALID_SCHEMA", "unsupported bundle schema")
                return True
            if report.check("BundleSchema", shape):
                if scope_valid and scope["baselineEvidence"] is not None:
                    report.check("ScopeBundleBinding", lambda: require(digest(bundle_data) == scope["baselineEvidence"]["sha256"] and
                        contained(repo, repo / relative_path(scope["baselineEvidence"]["path"])) == baseline_evidence,
                        "BINDING_MISMATCH", "scope references a different bundle"))
                refs, records, raw = {}, {}, {}
                seen = set()
                for key in BUNDLE_REFS:
                    def read(key=key):
                        require(type(bundle[key]) is dict and bundle[key].get("root") == "evidence", "UNSAFE_PATH", f"{key} must use evidence root")
                        path, data = read_ref(bundle[key], roots, key, read_data=key != "baselineTrace")
                        normalized = str(path).casefold()
                        require(normalized not in seen, "DUPLICATE_PATH", "bundle references alias the same file")
                        seen.add(normalized)
                        return path, data
                    value = report.check(f"File_{key}", read)
                    if value is not None:
                        path, data = value
                        refs[key] = path
                        report.data["inputPaths"][key] = str(path)
                        preserved[path] = bundle[key]["sha256"]
                        report.data["inputHashes"][key] = bundle[key]["sha256"]
                        if data is not None:
                            raw[key] = data
                        if key != "baselineTrace":
                            record = report.check(f"Json_{key}", lambda d=data, k=key: strict_json(d, k))
                            if record is not None:
                                records[key] = record
                candidate = records.get("candidate")
                if candidate is not None:
                    checked = report.check("CandidateSchema", lambda: (validate_candidate(candidate), True)[1])
                    if checked:
                        if head:
                            report.check("CandidateSourceAncestry", lambda: commit_ancestor(repo, candidate["sourceRevision"], head))
                        for key, field in (("draft", "draftPath"), ("soak", "soakPath"), ("laneEvidence", "laneEvidencePath"), ("finalFast", "finalFastPath")):
                            if key in refs:
                                def check_path(k=key, f=field):
                                    original = Path(candidate[f])
                                    original = original if original.is_absolute() else repo / original
                                    require(contained(evidence, original) == refs[k], "BINDING_MISMATCH", f"candidate {f} resolves to another record")
                                report.check(f"CandidatePath_{key}", check_path)
                        for key, path, field in (("legacyScope", LEGACY_SCOPE, "scopeManifestSha256"), ("legacyContentManifest", LEGACY_CONTENT, "contentManifestSha256")):
                            if key in raw and head:
                                def verify_blob(k=key, p=path, f=field):
                                    blob = git(repo, "show", f"{candidate['sourceRevision']}:{p}")
                                    require(digest(blob) == digest(raw[k]) == candidate[f] == preserved.get(repo / p),
                                            "BINDING_MISMATCH", f"committed/current/evidence {k} hashes differ")
                                report.check(f"CommittedBlob_{key}", verify_blob)
                        if "draft" in records:
                            report.check("PackagedDraft", lambda: validate_draft(records["draft"], candidate))
                        if "laneEvidence" in records:
                            report.check("LegacyLaneShape", lambda: validate_lanes(records["laneEvidence"], candidate))
                        if "soak" in records:
                            report.check("LegacySoakShape", lambda: validate_lanes(records["soak"], candidate, True))
                        if "finalFast" in records:
                            report.check("LegacyFinalFast", lambda: validate_final_fast(records["finalFast"]))
                        for key in ("operations", "visual", "humanReview", "baselineObservations", "anchorSurvey"):
                            if key in records:
                                report.check(f"Binding_{key}", lambda k=key: validate_binding(records[k], candidate, k))
                        if "packageManifest" in records and "buildReceipt" in records:
                            report.check("PackageInventory", lambda: validate_package(records["packageManifest"], records["buildReceipt"], bundle["packageIdentity"], candidate, raw["packageManifest"], roots, preserved))
                        from gameplay_baseline_readers import References, validate_visual, validate_human
                        reader_refs = References(roots, preserved, report)
                        if "visual" in records and "laneEvidence" in records:
                            report.check("RenderedVisualReceipts", lambda: validate_visual(records["visual"], candidate, records["laneEvidence"], reader_refs))
                        if "humanReview" in records and "operations" in records:
                            report.check("IndependentHumanAttestation", lambda: validate_human(records["humanReview"], candidate, bundle, records, reader_refs))
                if scope_valid:
                    for key in MEASURED_REFS:
                        if scope[key] is not None:
                            report.check(f"ScopeMeasuredBinding_{key}", lambda k=key: require(scope[k] == bundle[k], "BINDING_MISMATCH", f"scope {k} reference differs from bundle"))
    for name, implemented in CAPABILITIES.items():
        if not implemented:
            report.block(name, "VALIDATOR_CAPABILITY_MISSING", f"Required semantic/provenance reader not implemented: {name}")
    for path, old_hash in preserved.items():
        report.check(f"Immutable_{path.name}", lambda p=path, h=old_hash: require(file_digest(p)[1] == h, "LEGACY_ARTIFACT_CHANGED", f"input changed during audit: {p}"))
    report.data["limitations"] = ["Integrity is not proof of human play, consent or independent review.",
                                 "No gameplay runtime stage or Day 40 packaged producer is implemented here."]
    return report.finish()


def native_completion(log_text: str, test_count: int) -> tuple[str, int]:
    """Recognize either UE Quit's banner or the ordered no-Quit TestExit handshake."""
    completed = list(re.finditer(r"\*\*\*\* TEST COMPLETE\. EXIT CODE: (-?\d+) \*\*\*\*", log_text))
    queue = list(re.finditer(r"LogAutomationCommandLine: Display: \.\.\.Automation Test Queue Empty (\d+) tests performed\.", log_text))
    test_exit = list(re.finditer(r"LogExit: Display: \*\*\*\* TestExit: Automation Test Queue Empty \*\*\*\*", log_text))
    exits = list(re.finditer(r"LogWindows: FPlatformMisc::RequestExitWithStatus\(([01]), (-?\d+), ([^)\r\n]+)\)", log_text))
    require(log_text.count("TEST COMPLETE. EXIT CODE:") == len(completed) and
            log_text.count("LogWindows: FPlatformMisc::RequestExitWithStatus(") == len(exits) and
            all(match.group(2) == "0" for match in exits),
            "NATIVE_COMPLETION_CONFLICT", "malformed or nonzero native exit marker")
    queue_markers = log_text.count("LogAutomationCommandLine: Display: ...Automation Test Queue Empty")
    test_exit_markers = log_text.count("LogExit: Display: **** TestExit:")
    if completed:
        require(len(completed) == 1 and completed[0].group(1) == "0" and
                queue_markers == test_exit_markers == 0 and len(exits) <= 1 and
                (not exits or completed[0].end() < exits[0].start()),
                "NATIVE_COMPLETION_CONFLICT", "duplicate, conflicting or reordered explicit completion markers")
        return "ExplicitTestComplete", completed[0].start()
    require(queue_markers == len(queue) == test_exit_markers == len(test_exit) == len(exits) == 1,
            "NATIVE_COMPLETION_MISSING", "no-Quit completion needs one queue count, TestExit marker and exit request")
    require(int(queue[0].group(1)) == test_count,
            "NATIVE_COMPLETION_COUNT_MISMATCH", "queue performed-test count differs from report/discovery")
    require(exits[0].groups() == ("1", "0", "FEngineLoop::Tick.GScopedTestExit") and
            queue[0].end() < test_exit[0].start() and test_exit[0].end() < exits[0].start(),
            "NATIVE_COMPLETION_CONFLICT", "no-Quit completion has a conflicting call site or reordered handshake")
    return "QueueEmptyTestExit", queue[0].start()


def native(index: Path, log: Path, exit_code: int, started_at: str, namespace: str = "Aura.RoleBattle.",
           report_root: Path | None = None) -> dict:
    report = Report("native")
    def validate():
        start = utc(started_at, "started-at")
        require(start <= datetime.now(timezone.utc), "INVALID_TIMESTAMP", "runner start is in the future")
        export_root = root_path(report_root or index.parent)
        index_path = contained(export_root, root_path(index))
        require(index_path.name == "index.json", "NATIVE_REPORT_INVALID", "expected UE index.json")
        log_path = root_path(log)
        report.data["roots"] = {"report": str(export_root)}
        report.data["inputPaths"] = {"index": str(index_path), "log": str(log_path)}
        require(log_path != index_path, "NATIVE_REPORT_INVALID", "report and log must differ")
        index_data, log_data = file_bytes(index_path), file_bytes(log_path)
        require(index_path.stat().st_mtime >= start.timestamp() and log_path.stat().st_mtime >= start.timestamp(),
                "STALE_NATIVE_REPORT", "report/log predates this runner start")
        require(type(exit_code) is int and exit_code == 0, "NATIVE_CHILD_FAILED", "native child exit is nonzero")
        document = strict_json(index_data, "native index")
        log_text = log_data.decode("utf-8-sig")
        prefix = string(namespace, "namespace").rstrip(".") + "."
        require(re.fullmatch(r"[A-Za-z0-9_]+(?:\.[A-Za-z0-9_]+)+\.", prefix) is not None,
                "INVALID_NAMESPACE", "invalid native namespace")
        tests = document.get("tests")
        require(type(tests) is list and bool(tests), "EMPTY_NATIVE_REPORT", "native discovery is empty")
        names, succeeded, warned = set(), 0, 0
        for test in tests:
            obj(test, "native test", {"fullTestPath", "state", "warnings", "errors", "duration"})
            name = string(test["fullTestPath"], "fullTestPath")
            require(name.startswith(prefix) and name not in names, "NATIVE_NAMESPACE_MISMATCH", "unrelated or duplicate native test")
            names.add(name)
            errors = integer(test["errors"], "native errors")
            warnings = integer(test["warnings"], "native warnings")
            number(test["duration"], "native duration")
            require(test["state"] == "Success" and errors == 0, "NATIVE_TEST_FAILED", f"native test did not pass: {name}")
            if warnings:
                warned += 1
            else:
                succeeded += 1
        for field, expected in {"succeeded": succeeded, "succeededWithWarnings": warned, "failed": 0, "notRun": 0, "inProcess": 0}.items():
            require(integer(document.get(field), field) == expected, "NATIVE_AGGREGATE_MISMATCH", f"native {field} does not match tests")
        discovered = re.findall(r"Found (\d+) automation tests based on '([^']+)'", log_text)
        matched = [(int(count), group) for count, group in discovered if group.rstrip(".") + "." == prefix]
        require(len(matched) == 1 and matched[0][0] == len(tests) and len(discovered) == 1,
                "NATIVE_DISCOVERY_MISMATCH", "fresh log discovery does not match exact report namespace/count")
        completion_mode, completion_position = native_completion(log_text, len(tests))
        result_matches = list(re.finditer(r"Test Completed\. Result=\{([^}]+)\} Name=\{[^}]*\} Path=\{([^}]+)\}", log_text))
        log_results = [match.groups() for match in result_matches]
        require(len(log_results) == len(tests) and {path for _, path in log_results} == names and
                all(state in ("Success", "成功") for state, _ in log_results) and
                all(match.end() < completion_position for match in result_matches),
                "NATIVE_LOG_RESULT_MISMATCH", "fresh log per-test results differ from export")
        require(not re.search(r"Fatal error:|Assertion failed:|Unhandled Exception|Ensure condition failed", log_text),
                "NATIVE_CRASH_SIGNATURE", "native log contains a crash/assertion signature")
        report.data.update({"expectedTestCount": matched[0][0], "observedTestCount": len(tests),
                             "warningTestCount": warned, "namespace": prefix, "childExitCode": exit_code,
                             "completionMode": completion_mode,
                            "nativeTestsStatus": "PASS", "runnerStartedAtUtc": start.isoformat(),
                            "inputHashes": {"index": digest(index_data), "log": digest(log_data)}})
        warnings, errors = [], []
        for line_number, line in enumerate(log_text.splitlines(), 1):
            severity = re.search(r"\bLog[A-Za-z0-9_]+:\s*(Warning|Error):", line)
            if severity:
                # References preserve privacy; the explicitly hashed source log
                # retains full text for authorized review without duplicating IDs.
                target = warnings if severity.group(1) == "Warning" else errors
                target.append({"lineNumber": line_number, "lineSha256": digest(line.encode("utf-8"))})
        report.data.update({"logWarningCount": len(warnings), "logErrorCount": len(errors),
                            "warningLogLines": warnings[:100], "errorLogLines": errors[:100],
                            "logLineReferencesTruncated": len(warnings) > 100 or len(errors) > 100})
        require(not errors, "NATIVE_LOG_ERROR", f"{len(errors)} UE error log lines require explicit investigation/classification")
        if warned or warnings:
            import gameplay_native_warnings
            classification = gameplay_native_warnings.classify(log_text, tests)
            classification["policySha256"] = file_digest(Path(gameplay_native_warnings.__file__))[1]
            report.data["warningClassification"] = classification
            if classification["status"] != "PASS":
                report.block("NativeWarnings", "NATIVE_WARNINGS_UNREVIEWED", f"{warned} successful tests and {len(warnings)} log warning lines do not match the reviewed negative-case policy")
        return True
    passed = report.check("FreshNativeReport", validate)
    report.data["toolingStatus"] = "PASS" if passed else "FAIL"
    return report.finish()


def result_exit(result: dict) -> int:
    return {"PASS": 0, "FAIL": 1, "BLOCKED": 2, "DIAGNOSTIC": 2, "TIMEOUT": 3, "CLEANUP_FAILURE": 3}[result["status"]]


def write_result(path: Path, result: dict, protected: list[Path]) -> None:
    path = root_path(path)
    require(all(path != root_path(item) for item in protected), "OUTPUT_COLLISION", "output cannot overwrite an input")
    path.parent.mkdir(parents=True, exist_ok=True)
    # Exclusive create prevents stale output reuse and concurrent overwrite.
    with path.open("x", encoding="utf-8", newline="\n") as handle:
        json.dump(result, handle, indent=2, ensure_ascii=True, allow_nan=False)
        handle.write("\n")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    audit_parser = commands.add_parser("audit")
    audit_parser.add_argument("--repo-root", type=Path, required=True)
    audit_parser.add_argument("--scope", type=Path, required=True)
    for switch in ("baseline-evidence", "evidence-root", "package-root"):
        audit_parser.add_argument(f"--{switch}", type=Path)
    audit_parser.add_argument("--output", type=Path)
    native_parser = commands.add_parser("native")
    native_parser.add_argument("--index", type=Path, required=True)
    native_parser.add_argument("--log", type=Path, required=True)
    native_parser.add_argument("--exit-code", type=int, required=True)
    native_parser.add_argument("--started-at", required=True)
    native_parser.add_argument("--namespace", default="Aura.RoleBattle.")
    native_parser.add_argument("--report-root", type=Path)
    native_parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    protected = []
    try:
        if args.command == "audit":
            protected = [args.scope, args.repo_root / LEGACY_SCOPE, args.repo_root / LEGACY_CONTENT]
            if args.baseline_evidence:
                protected.append(args.baseline_evidence)
            result = audit(args.repo_root, args.scope, args.baseline_evidence, args.evidence_root, args.package_root)
        else:
            protected = [args.index, args.log]
            result = native(args.index, args.log, args.exit_code, args.started_at, args.namespace, args.report_root)
    except (EvidenceError, OSError, ValueError, TypeError, KeyError, RecursionError) as exc:
        failure_status = getattr(exc, "status", "FAIL")
        result = {"schemaVersion": 1, "command": args.command, "status": failure_status, "evidenceStatus": failure_status,
                  "toolingStatus": "NOT_RUN", "runtimeEntry": "BLOCKED_RUNTIME_ENTRY", "checks": [],
                  "capabilities": dict(CAPABILITIES), "reasonCode": getattr(exc, "code", "AUDIT_ERROR"), "message": str(exc)}
    if args.output is None:
        print(json.dumps(result, indent=2, ensure_ascii=True, allow_nan=False))
        return result_exit(result)
    try:
        write_result(args.output, result, protected)
    except (EvidenceError, OSError) as exc:
        print(json.dumps({"status": "FAIL", "reasonCode": getattr(exc, "code", "OUTPUT_ERROR"), "message": str(exc)}), file=sys.stderr)
        return 1
    print(json.dumps({"status": result["status"], "reasonCode": result["reasonCode"], "runtimeEntry": result["runtimeEntry"], "output": str(args.output)}))
    return result_exit(result)


if __name__ == "__main__":
    raise SystemExit(main())
