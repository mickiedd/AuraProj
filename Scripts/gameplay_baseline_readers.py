"""Strict readers for collected baseline support evidence; never invent receipts.

Archives are read without extraction. Images are decoded, not merely identified
by a header. Human review is attributed testimony, never machine proof of play.
"""
from __future__ import annotations

import hashlib
import io
from pathlib import Path
import stat
import time
import warnings
import zipfile
import zlib

import gameplay_expansion as ge


REVIEW_DISPOSITIONS = {
    "journey", "visuals", "privacy", "economyPersistence", "warnings",
    "operationsIndependence", "baselineSurvey",
}
VISUAL_STATES = {"login-loading", "role-hud", "attack", "firearm", "trade", "death-recovery", "reconnect"}


class BoundedArchiveFile:
    """Bound central-directory reads before ZipFile materializes ZipInfo objects."""
    def __init__(self, handle):
        self.handle = handle
        self.handle.seek(0, 2)
        self.size = self.handle.tell()
        self.handle.seek(0)

    def read(self, size=-1):
        if size < 0:
            size = max(0, self.size - self.handle.tell())
        ge.require(size <= 64 * 1024 * 1024, "ARCHIVE_LIMIT", "archive metadata/read exceeds 64 MiB parser budget")
        return self.handle.read(size)

    def seek(self, offset, whence=0):
        return self.handle.seek(offset, whence)

    def tell(self):
        return self.handle.tell()

    def seekable(self):
        return self.handle.seekable()


def fields(record, label, required, optional=()):
    return ge.obj(record, label, set(required), set(required) | set(optional) | {"extensions"})


def sequence(value, label, minimum=1):
    ge.require(type(value) is list and len(value) >= minimum, "INVALID_TYPE", f"{label} needs >= {minimum} entries")
    return value


def interval(record, label, maximum=None):
    start = ge.utc(record.get("startedAtUtc"), label + ".startedAtUtc")
    end = ge.utc(record.get("completedAtUtc"), label + ".completedAtUtc")
    ge.require(end >= start, "INVALID_INTERVAL", f"{label}: end precedes start")
    if "durationSeconds" in record:
        duration = ge.number(record["durationSeconds"], label + ".durationSeconds")
        ge.require(abs((end - start).total_seconds() - duration) <= 1.0,
                   "DURATION_MISMATCH", f"{label}: wall/monotonic duration disagree")
    if maximum is not None:
        ge.require((end - start).total_seconds() <= maximum, "DURATION_LIMIT", f"{label} exceeds {maximum}s")
    return start, end


class References:
    """Retain every transitive file hash for the auditor's end-of-run recheck."""

    def __init__(self, roots, preserved, report=None):
        self.roots, self.preserved, self.report = roots, preserved, report

    def read(self, ref, label, data=True, root="evidence"):
        ge.require(type(ref) is dict and ref.get("root") == root, "UNSAFE_PATH", f"{label} must use {root} root")
        path, content = ge.read_ref(ref, self.roots, label, read_data=data)
        previous = self.preserved.get(path)
        ge.require(previous is None or previous == ref["sha256"], "BINDING_MISMATCH", f"{label}: conflicting file reference")
        self.preserved[path] = ref["sha256"]
        if self.report is not None:
            self.report.data.setdefault("transitiveInputs", {})[str(path)] = ref["sha256"]
        return path, content

    def json(self, ref, label):
        return ge.strict_json(self.read(ref, label)[1], label)

    def evidence(self, refs, label):
        seen = set()
        for index, ref in enumerate(sequence(refs, label)):
            path, _ = self.read(ref, f"{label}[{index}]", data=False)
            ge.require(path not in seen, "DUPLICATE_PATH", f"{label}: repeated evidence")
            seen.add(path)


def producer(record, refs, label):
    """Supported receipt format, not a claim that its author is trusted."""
    ext = ge.obj(record.get("extensions"), label + ".extensions", {"producer"})
    meta = fields(ext["producer"], label + ".producer", {"producerId", "producerVersion", "source"})
    if meta["producerId"] != "AuraBaselineReceipt" or type(meta["producerVersion"]) is not int or meta["producerVersion"] != 1:
        raise ge.EvidenceError("UNSUPPORTED_EVIDENCE_PRODUCER", f"{label}: no reader for this producer format", "BLOCKED")
    refs.read(meta["source"], label + ".producer.source", data=False)


def support(record, kind, candidate, refs, extra):
    fields(record, kind, {"schemaVersion", "kind", *ge.BINDING, "status", "startedAtUtc", "completedAtUtc", "evidence", *extra})
    ge.require(type(record["schemaVersion"]) is int and record["schemaVersion"] == 1 and record["kind"] == kind,
               "INVALID_SCHEMA", f"{kind}: unsupported schema")
    ge.validate_binding(record, candidate, kind)
    ge.require(record["status"] == "PASS", "SUPPORT_NOT_PASS", f"{kind}: support record is not PASS")
    interval(record, kind)
    refs.evidence(record["evidence"], kind + ".evidence")
    producer(record, refs, kind)


def archive_inventory(manifest, receipt, identity, refs):
    """Version 1 supports ZIP (stored/deflate) with exact relative file coverage.

    Read bounded chunks, never extract or follow archive links. Inventory sizes
    bound decompression; CRC and SHA-256 are checked against extracted bytes.
    """
    if identity["kind"] == "manifest-bytes-v1":
        ge.require(identity["archive"] is None, "PACKAGE_IDENTITY_UNPROVEN", "manifest identity cannot also name an archive")
        return
    ge.require(identity["kind"] == "archive-bytes-v1", "PACKAGE_IDENTITY_UNPROVEN", "unsupported identity")
    ext = ge.obj(receipt.get("extensions", {}), "buildReceipt.extensions")
    if ext.get("archiveFormat") != "zip-v1":
        raise ge.EvidenceError("UNSUPPORTED_ARCHIVE_FORMAT", "archive reader supports explicit zip-v1 only", "BLOCKED")
    path, _ = refs.read(identity["archive"], "archive", data=False, root="package")
    ge.require(identity["sha256"] == identity["archive"]["sha256"], "EVIDENCE_HASH_MISMATCH", "archive identity mismatch")
    subtree = ge.contained(refs.roots["package"], refs.roots["package"] / ge.relative_path(receipt["packageSubdirectory"]))
    ge.require(subtree != path and subtree not in path.parents, "UNSAFE_PATH", "archive must be outside extracted inventory")
    expected = {}
    for entry in sequence(manifest.get("entries"), "entries"):
        name = ge.relative_path(entry["path"]).as_posix().casefold()
        ge.require(name not in expected, "DUPLICATE_PATH", "duplicate archive inventory entry")
        ge.integer(entry["sizeBytes"], "archive entry.sizeBytes")
        ge.sha(entry["sha256"], "archive entry.sha256")
        expected[name] = entry
    started = time.monotonic()
    try:
        with path.open("rb") as handle, zipfile.ZipFile(BoundedArchiveFile(handle)) as archive:
            members = archive.infolist()
            ge.require(len(members) <= 200000, "ARCHIVE_LIMIT", "archive has too many entries")
            seen, files, directories = set(), set(), set()
            for member in members:
                if time.monotonic() - started > 120:
                    raise ge.EvidenceError("ARCHIVE_TIMEOUT", "archive validation exceeded 120s", "TIMEOUT")
                # ZipInfo truncates .filename at NUL; reject the original too.
                ge.require(member.orig_filename == member.filename and "\\" not in member.filename,
                           "UNSAFE_PATH", "noncanonical archive member")
                name = ge.relative_path(member.filename.rstrip("/")).as_posix().casefold()
                ge.require(name not in seen, "DUPLICATE_PATH", "duplicate/case-aliased archive member")
                seen.add(name)
                mode = member.external_attr >> 16
                ge.require(stat.S_IFMT(mode) in (0, stat.S_IFREG, stat.S_IFDIR), "UNSAFE_PATH", "archive links/devices are forbidden")
                if member.flag_bits & 1 or member.compress_type not in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED):
                    raise ge.EvidenceError("UNSUPPORTED_ARCHIVE_FORMAT", "encrypted or unsupported compression", "BLOCKED")
                if member.is_dir():
                    ge.require(member.file_size == 0 and stat.S_IFMT(mode) != stat.S_IFREG,
                               "INVALID_ARCHIVE", "invalid directory member")
                    directories.add(name)
                    continue
                ge.require(stat.S_IFMT(mode) != stat.S_IFDIR, "INVALID_ARCHIVE", "file declared as a directory")
                ge.require(name in expected, "PACKAGE_INVENTORY_INCOMPLETE", "unlisted archive file")
                entry = expected[name]
                ge.require(member.file_size == entry["sizeBytes"], "EVIDENCE_HASH_MISMATCH", "archive size differs from inventory")
                count, hasher = 0, hashlib.sha256()
                with archive.open(member) as handle:
                    while True:
                        chunk = handle.read(1024 * 1024)
                        if not chunk:
                            break
                        count += len(chunk)
                        ge.require(count <= entry["sizeBytes"], "ARCHIVE_LIMIT", "archive exceeds declared size")
                        hasher.update(chunk)
                        if time.monotonic() - started > 120:
                            raise ge.EvidenceError("ARCHIVE_TIMEOUT", "archive validation exceeded 120s", "TIMEOUT")
                ge.require(count == entry["sizeBytes"] and hasher.hexdigest() == entry["sha256"],
                           "EVIDENCE_HASH_MISMATCH", "archive bytes differ from extracted inventory")
                files.add(name)
            ge.require(files == set(expected), "PACKAGE_INVENTORY_INCOMPLETE", "archive file coverage differs")
            for directory in directories:
                ge.require(any(name.startswith(directory + "/") for name in files),
                           "PACKAGE_INVENTORY_INCOMPLETE", "unlisted empty directory in archive")
            for name in files:
                ge.require(not any(parent.as_posix().casefold() in files for parent in Path(name).parents if str(parent) != "."),
                           "UNSAFE_PATH", "archive file/directory collision")
    except (zipfile.BadZipFile, NotImplementedError, RuntimeError, EOFError, zlib.error) as exc:
        raise ge.EvidenceError("INVALID_ARCHIVE", str(exc)) from exc


def decode_image(ref, refs):
    path, data = refs.read(ref, "rendered capture")
    try:
        from PIL import Image
    except ImportError as exc:
        raise ge.EvidenceError("IMAGE_DECODER_MISSING", "Pillow is required to validate rendered captures", "BLOCKED") from exc
    try:
        with warnings.catch_warnings():
            warnings.simplefilter("error", Image.DecompressionBombWarning)
            with Image.open(io.BytesIO(data)) as image:
                ge.require(image.format in ("PNG", "JPEG"), "UNSUPPORTED_IMAGE_FORMAT", "only PNG/JPEG captures supported")
                ge.require(0 < image.width <= 8192 and 0 < image.height <= 8192 and image.width * image.height <= 33554432,
                           "IMAGE_LIMIT", "capture dimensions exceed decoder budget")
                ge.require(getattr(image, "n_frames", 1) == 1, "INVALID_IMAGE", "animated capture is not a frame")
                image.verify()
            with Image.open(io.BytesIO(data)) as image:
                image.load()
                return image.size
    except (OSError, ValueError, SyntaxError, Image.DecompressionBombWarning, Image.DecompressionBombError) as exc:
        raise ge.EvidenceError("INVALID_IMAGE", f"capture does not decode: {path.name}") from exc


def validate_visual(record, candidate, lanes, refs):
    support(record, "PlayableCandidateVisual", candidate, refs, {"captures"})
    start, end = interval(record, "visual")
    expected = {(lane, state) for lane in ge.OLD_LANES for state in VISUAL_STATES}
    seen = set()
    seen_images = set()
    lane_map = {lane["laneId"]: lane for lane in lanes["lanes"]}
    for capture in sequence(record["captures"], "captures"):
        fields(capture, "capture", {"laneId", "state", "status"},
               {"reason", "image", "resolution", "quality", "renderer", "capturedAtUtc", "processId"})
        lane, state = capture["laneId"], capture["state"]
        key = (ge.string(lane, "capture lane"), ge.string(state, "capture state"))
        ge.require(key in expected and key not in seen, "VISUAL_COVERAGE", "unknown/duplicate capture state")
        seen.add(key)
        if ge.OLD_LANES[lane][0] == "Aura" and state == "firearm":
            ge.require(capture["status"] == "NotApplicable" and capture.get("reason") == "Aura has no firearm",
                       "VISUAL_COVERAGE", "Aura firearm requires explicit role reason")
            ge.require(set(capture) <= {"laneId", "state", "status", "reason", "extensions"},
                       "VISUAL_COVERAGE", "NotApplicable cannot carry alleged capture evidence")
            continue
        ge.require(capture["status"] == "PASS", "VISUAL_COVERAGE", "required capture is not PASS")
        ge.require(capture.get("renderer") in ("D3D11", "D3D12", "Vulkan"), "UNRENDERED_CAPTURE", "capture needs real RHI")
        resolution = sequence(capture.get("resolution"), "resolution", 2)
        ge.require(len(resolution) == 2 and all(type(n) is int and n > 0 for n in resolution), "INVALID_TYPE", "integer dimensions required")
        ge.require(capture.get("quality") in ("Low", "Medium", "High", "Epic"), "INVALID_SCHEMA", "quality setting missing")
        captured = ge.utc(capture.get("capturedAtUtc"), "capturedAtUtc")
        ge.require(start <= captured <= end, "CAPTURE_PROVENANCE", "capture outside collection interval")
        pid = ge.integer(capture.get("processId"), "processId", 1)
        processes = lane_map[lane]["execution"]["processes"]
        matches = [p for p in processes if p.get("pid") == pid]
        ge.require(len(matches) == 1 and matches[0].get("role") in ("listen-host", "client"),
                   "CAPTURE_PROVENANCE", "capture must come from exactly one rendered lane process")
        process = matches[0]
        ge.integer(process["pid"], "capture process.pid", 1)
        arguments = sequence(process.get("arguments"), "capture process.arguments", 0)
        ge.require(all(type(arg) is str for arg in arguments), "INVALID_TYPE", "launch arguments must be strings")
        pstart, pend = interval(process, "capture process")
        ge.require(pstart <= captured <= pend and not any(arg.lower().split("=", 1)[0] == "-nullrhi" for arg in arguments),
                   "UNRENDERED_CAPTURE", "capture contradicts process interval/renderer")
        ge.require(decode_image(capture.get("image"), refs) == tuple(resolution), "CAPTURE_DIMENSION_MISMATCH", "capture dimensions disagree")
        image_path = capture["image"]["path"].replace("\\", "/").casefold()
        ge.require(image_path not in seen_images, "DUPLICATE_PATH", "each required capture needs its own frame reference")
        seen_images.add(image_path)
    ge.require(seen == expected, "VISUAL_COVERAGE", "missing required lane/state capture")


def validate_human(record, candidate, bundle, records, refs):
    extra = {"implementerId", "reviewerId", "implementerEnvironmentId", "operatorId", "operatorEnvironmentId",
             "authorizedCollection", "reviewedAtUtc", "reviewedHashes", "dispositions", "findings", "blockingFindings"}
    support(record, "GameplayBaselineHumanReview", candidate, refs, extra)
    for name in ("implementerId", "reviewerId", "implementerEnvironmentId", "operatorId", "operatorEnvironmentId"):
        ge.validate_run_id(record[name])
    ge.require(record["implementerId"] != record["reviewerId"] and record["implementerId"] != record["operatorId"] and
               record["implementerEnvironmentId"] != record["operatorEnvironmentId"],
               "REVIEW_NOT_INDEPENDENT", "review/operator/environment must be distinct from implementer")
    ge.require(record["authorizedCollection"] is True, "HUMAN_REVIEW_MISSING", "authorized collection attestation is required")
    start, end = interval(record, "human review")
    reviewed = ge.utc(record["reviewedAtUtc"], "reviewedAtUtc")
    ge.require(start <= reviewed <= end, "REVIEW_TIMESTAMP", "review time outside review interval")
    ge.require(ge.utc(candidate["finalizedAtUtc"], "candidate.finalizedAtUtc") <= reviewed,
               "REVIEW_TIMESTAMP", "review predates final candidate")
    required_hashes = set(ge.BUNDLE_REFS) - {"humanReview", "legacyScope", "legacyContentManifest", "finalFast"}
    covered = fields(record["reviewedHashes"], "reviewedHashes", required_hashes)
    ge.require(set(covered) == required_hashes, "REVIEW_COVERAGE", "review cannot omit records or include self-references")
    for key in required_hashes:
        ge.require(ge.sha(covered[key], key) == bundle[key]["sha256"], "REVIEW_COVERAGE", f"review references other {key} bytes")
    for key, source in records.items():
        if key in ("humanReview", "legacyScope", "legacyContentManifest", "finalFast"):
            continue
        stamps = [source[k] for k in ("completedAtUtc", "finalizedAtUtc", "createdAtUtc", "capturedAtUtc", "surveyedAtUtc") if k in source]
        for stamp in stamps:
            ge.require(ge.utc(stamp, key) <= reviewed, "REVIEW_TIMESTAMP", "review predates evidence it claims to inspect")
    dispositions = fields(record["dispositions"], "review dispositions", REVIEW_DISPOSITIONS)
    ge.require(set(dispositions) == REVIEW_DISPOSITIONS and all(value == "PASS" for value in dispositions.values()),
               "HUMAN_REVIEW_BLOCKED", "all required review dispositions must pass")
    ge.require(type(record["blockingFindings"]) is list and not record["blockingFindings"], "HUMAN_REVIEW_BLOCKED", "unresolved blocking findings")
    ids = set()
    for finding in sequence(record["findings"], "findings", 0):
        fields(finding, "finding", {"id", "severity", "status", "contractBlocking", "summary"}, {"resolutionEvidence"})
        finding_id = ge.string(finding["id"], "finding.id")
        ge.require(finding_id not in ids, "DUPLICATE_FINDING", "repeated finding")
        ids.add(finding_id)
        ge.string(finding["summary"], "finding.summary")
        ge.require(finding["severity"] in ("P0", "P1", "P2", "P3") and finding["status"] in ("Open", "Resolved") and
                   type(finding["contractBlocking"]) is bool, "INVALID_SCHEMA", "invalid finding disposition")
        blocking = finding["severity"] in ("P0", "P1") or finding["contractBlocking"]
        ge.require(not blocking or finding["status"] == "Resolved", "HUMAN_REVIEW_BLOCKED", "unresolved contract-blocking finding")
        if finding["status"] == "Resolved":
            refs.evidence(finding.get("resolutionEvidence"), "finding resolution evidence")
    ops = records["operations"]
    ge.require(ops.get("operatorId") == record["operatorId"] and ops.get("operatorEnvironmentId") == record["operatorEnvironmentId"],
               "REVIEW_NOT_INDEPENDENT", "operations attestation does not identify reviewed operator/environment")
