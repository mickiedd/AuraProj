"""Validate behavioral invariants in a Crunch combo network report/timeline."""

from __future__ import annotations

import argparse
import csv
import json
from collections import defaultdict
from pathlib import Path


def _number(row: dict[str, str], key: str) -> float | None:
    value = row.get(key, "")
    return float(value) if value not in (None, "") else None


def validate(report_path: Path) -> list[str]:
    errors: list[str] = []
    report = json.loads(report_path.read_text(encoding="utf-8"))
    if report.get("schemaVersion") != 1:
        errors.append("schemaVersion must be 1")
    if report.get("passed") is not True:
        errors.append("report passed must be true")
    mode = report.get("Mode", report.get("Topology", ""))
    event_source = report.get("eventSource")
    if mode == "Listen" and event_source != "Authored":
        errors.append("listen reports must use Authored eventSource")
    if mode == "Dedicated" and event_source != "DedicatedFallback":
        errors.append("dedicated reports must use DedicatedFallback eventSource")
    if mode == "Listen" and "NetMode" in report and report.get("NetMode") != "ListenServer":
        errors.append("listen reports must identify NetMode=ListenServer")
    if mode == "Dedicated" and "NetMode" in report and report.get("NetMode") != "DedicatedServer":
        errors.append("dedicated reports must identify NetMode=DedicatedServer")
    if ("ServerRole" in report or "ClientRole" in report) and (report.get("ServerRole") != "Authority" or report.get("ClientRole") != "AutonomousProxy"):
        errors.append("report must identify authority server and autonomous client roles")
    timeline_value = report.get("Artifacts", {}).get("TimelineCsv")
    if not timeline_value:
        return errors + ["Artifacts.TimelineCsv is missing"]
    timeline_path = Path(timeline_value)
    if not timeline_path.is_absolute():
        timeline_path = report_path.parent / timeline_path
    if not timeline_path.exists():
        return errors + [f"timeline does not exist: {timeline_path}"]

    with timeline_path.open(newline="", encoding="utf-8-sig") as stream:
        rows = list(csv.DictReader(stream))
    if not rows:
        return errors + ["timeline is empty"]
    peers: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        peers[row.get("Peer", "")].append(row)
    server = peers.get("Server", [])
    client = peers.get("Client", [])
    if not server or not client:
        errors.append("timeline must contain both Server and Client peers")

    scenario = report.get("scenario", "")
    server_events = [row.get("Event", "") for row in server]
    client_events = [row.get("Event", "") for row in client]
    terminal_server = server
    if scenario == "ComboCancel":
        cancel_index = next((index for index, row in enumerate(server) if row.get("Event") == "CANCEL_OBSERVED"), -1)
        if cancel_index >= 0:
            terminal_server = server[cancel_index + 1 :]
    terminal_events = [row.get("Event", "") for row in terminal_server]
    expected_sections = 2 if scenario == "ComboAtOrAfterClose" else 4
    if terminal_events.count("AuthoredOpen") + terminal_events.count("FallbackOpen") != expected_sections:
        errors.append(f"server must open exactly {expected_sections} terminal combo sections")
    if terminal_events.count("AuthoredClose") + terminal_events.count("FallbackClose") != expected_sections:
        errors.append(f"server must close exactly {expected_sections} terminal combo sections")
    if any(row.get("Event", "").lower().endswith("damage") for row in client):
        errors.append("client timeline contains authoritative damage")
    if scenario in {"ComboFull", "ComboBeforeClose", "ComboCancel"}:
        if not any(event in server_events for event in ("PASS", "CANCEL_PASS")):
            errors.append("server terminal pass marker is missing")
        if not any(event in client_events for event in ("COMPLETE", "CANCEL_COMPLETE")):
            errors.append("client terminal completion marker is missing")
    if scenario == "ComboAtOrAfterClose" and "NEARCLOSE_REJECTED" not in server_events:
        errors.append("AtOrAfterClose must record a server rejection marker")

    opens: dict[int, float] = {}
    closes: dict[int, float] = {}
    for row in terminal_server:
        event = row.get("Event", "")
        section_text = row.get("Section", "")
        if section_text in (None, ""):
            continue
        section = int(float(section_text))
        time = _number(row, "Time")
        if time is None:
            continue
        if event in {"AuthoredOpen", "FallbackOpen"}:
            opens[section] = time
        elif event in {"AuthoredClose", "FallbackClose"}:
            closes[section] = time
    for section in range(expected_sections):
        if section not in opens or section not in closes:
            errors.append(f"server section {section} is missing open/close timing")
        elif opens[section] >= closes[section]:
            errors.append(f"server section {section} close is not after open")

    accepted = [row for row in server if row.get("Event") == "InputDecision" and row.get("Decision") == "Accepted"]
    if len(accepted) > 3:
        errors.append("server accepted more than three successor inputs")
    for row in accepted:
        open_time = _number(row, "OpenTime")
        close_time = _number(row, "CloseTime")
        decision_time = _number(row, "Time")
        if open_time is None or decision_time is None:
            errors.append("accepted decision is missing open/decision timing")
        elif decision_time < open_time:
            errors.append("accepted decision precedes its open event")
        # CloseTime is the previous section's close boundary on successor
        # decisions. Compare only when it belongs to the current section.
        if close_time is not None and close_time >= open_time and decision_time >= close_time:
            errors.append("accepted decision occurs at/after its close boundary")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("report", type=Path, nargs="+")
    args = parser.parse_args()
    failures = []
    for report in args.report:
        errors = validate(report)
        if errors:
            failures.extend(f"{report}: {error}" for error in errors)
    if failures:
        print("Crunch network timeline semantics: FAIL")
        print("\n".join(f"- {failure}" for failure in failures))
        return 1
    print(f"Crunch network timeline semantics: PASS ({len(args.report)} report(s))")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
