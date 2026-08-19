"""Contract checks for UnrealEditor-client server routing."""

import ast
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANAGER = ROOT / "Scripts" / "GameServerManager.py"
CLIENT = ROOT / "Source" / "Aura" / "Private" / "Game" / "GameServerClient.cpp"
TRAVEL = ROOT / "Source" / "Aura" / "Private" / "Game" / "ServerTravelComponent.cpp"
EDITOR_PAIR_LOGS = (
    ROOT / "Saved" / "Logs" / "Editor-pair-server.log",
    ROOT / "Saved" / "Logs" / "Editor-pair-client.log",
)


def main() -> int:
    manager_source = MANAGER.read_text(encoding="utf-8")
    client_source = CLIENT.read_text(encoding="utf-8")
    travel_source = TRAVEL.read_text(encoding="utf-8")
    ast.parse(manager_source)

    for marker in (
        "clientExecutable",
        "clientEngineRoot",
        "clientNetworkChangelist",
        "clientNetworkVersion",
    ):
        assert marker in client_source, f"client request is missing {marker}"

    for marker in (
        "_is_editor_client",
        "_locate_editor_exe_for_client",
        "Restarting levelId",
        "serverMode",
    ):
        assert marker in manager_source, f"manager is missing {marker}"
    for marker in ("incompatible version", "RemoteNetworkVersion", "Client/server build mismatch"):
        assert marker in travel_source, f"client failure guidance is missing {marker}"

    checksums = []
    for log_path in EDITOR_PAIR_LOGS:
        if log_path.is_file():
            matches = re.findall(
                r"LogNetVersion: .*Checksum: (\d+)",
                log_path.read_text(encoding="utf-8", errors="replace"),
            )
            if matches:
                checksums.append(matches[-1])
    if checksums:
        assert len(checksums) == 2 and len(set(checksums)) == 1, checksums

    print("UnrealEditor-compatible server routing contract: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
