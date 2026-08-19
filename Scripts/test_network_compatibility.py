"""Contract checks for synchronized client/server network-version validation."""

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def checksum(path: Path):
    if not path.is_file():
        return None
    pattern = re.compile(r"LogNetVersion: .*Checksum: (\d+)")
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = pattern.search(line)
        if match:
            return match.group(1)
    return None


def main() -> int:
    runner = (ROOT / "RunRoleBattleDay7PackagedTopology.ps1").read_text(encoding="utf-8")
    assert "Get-NetworkVersion" in runner
    assert "NetworkVersionMatch" in runner
    assert "Rebuild and distribute one synchronized package" in runner

    server = checksum(ROOT / "Saved/Logs/Day07-Packaged-Server.log")
    client1 = checksum(ROOT / "Saved/Logs/Day07-Packaged-Client1.log")
    client2 = checksum(ROOT / "Saved/Logs/Day07-Packaged-Client2.log")
    assert server and client1 and client2, "packaged network checksum evidence is missing"
    assert server == client1 == client2, (server, client1, client2)
    print(f"Packaged client/server network compatibility: PASS (checksum {server})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
