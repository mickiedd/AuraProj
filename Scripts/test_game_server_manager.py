"""Focused contract checks for dedicated-server executable selection."""

import asyncio
import ast
import importlib.util
import json
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANAGER = ROOT / "Scripts" / "GameServerManager.py"


def load_manager_module():
    spec = importlib.util.spec_from_file_location("aura_game_server_manager", MANAGER)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> int:
    source = MANAGER.read_text(encoding="utf-8")
    tree = ast.parse(source)
    level_config = json.loads((ROOT / "Content/Config/LevelConfig.json").read_text(encoding="utf-8"))
    connection_config = json.loads((ROOT / "Content/Config/ServerConnection.json").read_text(encoding="utf-8"))
    client_source = (ROOT / "Source/Aura/Private/Game/GameServerClient.cpp").read_text(encoding="utf-8")
    game_mode_source = (ROOT / "Source/Aura/Private/Game/AuraGameModeBase.cpp").read_text(encoding="utf-8")
    startup_map = ROOT / "Content/Maps/StartupMap.umap"
    staged_server = ROOT / "Saved/StagedBuilds/WindowsServer/AuraServer.exe"

    assert startup_map.is_file(), "StartupMap source asset is missing"
    assert any(entry["mapPath"] == "/Game/Maps/StartupMap" for entry in level_config["levels"])
    assert "Saved" in source and "StagedBuilds" in source
    assert "has_cooked_payload" in source
    assert "Ignoring uncooked AuraServer binary" in source
    assert "locate_editor_exe" in source
    assert "_is_editor_client" in source
    assert "_locate_editor_exe_for_client" in source
    assert "clientEngineRoot" in source
    assert "editor_executable_name" in source
    assert "is_editor_executable" in source
    assert "persistence_provider" in source
    assert "--persistence-provider" in source
    assert "AURA_PERSISTENCE_PROVIDER" in source
    assert '"serverMode"' in source
    assert "AURA_GSM_AUTH_TOKEN" in source
    assert "AURA_GSM_SERVER_AUTH_TOKEN" in source
    assert "AURA_GSM_ALLOWED_CLIENTS" in source
    assert "AURA_GSM_SERVER_READY_NONCE" in source
    assert "is_authorized_request" in source
    assert "Port does not match configured level" in source
    assert "announced_host = self.public_host" in source
    assert connection_config["gameServerAddress"] == "127.0.0.1"
    assert r'\"authToken\"' in client_source
    assert "AURA_GSM_AUTH_TOKEN" in client_source
    assert r'\"serverAuthToken\"' in game_mode_source
    assert r'\"readyNonce\"' in game_mode_source
    assert "AURA_GSM_SERVER_READY_NONCE" in game_mode_source
    login_source = (ROOT / "Source/Aura/Private/Game/LoginPlayerController.cpp").read_text(encoding="utf-8")
    assert "AURA_GSM_ADDRESS" in login_source
    assert "AURA_GSM_ADDRESS" in game_mode_source
    assert '#include "Async/Async.h"' in login_source
    assert "auto ReleaseHandler = [HandleResponse]" in login_source
    assert "AsyncTask(ENamedThreads::GameThread" in login_source
    assert "QueryTimeout, HandleResponse, ReleaseHandler]()" in login_source
    assert login_source.count("*HandleResponse = FLoginGsmResponseHandler();") == 1
    handler_start = login_source.index("*HandleResponse = [")
    handler_end = login_source.index("\n\t};", handler_start)
    handler_source = login_source[handler_start:handler_end]
    assert "*HandleResponse = FLoginGsmResponseHandler();" not in handler_source
    assert "ReleaseHandler();" in handler_source

    manager = load_manager_module()
    assert manager.editor_executable_name("UnrealEditor-Win64-DebugGame.exe") == (
        "UnrealEditor-Win64-DebugGame.exe"
    )
    assert manager.editor_executable_name("UnrealEditor-Evil.exe") == "UnrealEditor.exe"
    assert manager.is_editor_executable(Path("UnrealEditor-Win64-DebugGame.exe"))
    assert manager.is_editor_executable(Path("UnrealEditor-Cmd.exe"))
    assert not manager.is_editor_executable(Path("AuraServer.exe"))

    loopback = manager.GameServerManager(
        "127.0.0.1", 0, "127.0.0.1", None, 0,
        auth_token="client-secret",
        server_auth_token="server-secret",
    )
    assert loopback.control_plane_policy_error() is None
    assert loopback.is_authorized_request(
        {"action": "request_server", "authToken": "client-secret"}, "127.0.0.1"
    )
    assert not loopback.is_authorized_request(
        {"action": "request_server", "authToken": "wrong"}, "127.0.0.1"
    )
    assert loopback.is_authorized_request(
        {"action": "server_ready", "serverAuthToken": "server-secret"}, "127.0.0.1"
    )
    assert not loopback.is_authorized_request(
        {"action": "server_ready", "authToken": "client-secret"}, "127.0.0.1"
    )

    unsafe_lan = manager.GameServerManager("0.0.0.0", 0, "192.168.1.6", None, 0)
    assert unsafe_lan.control_plane_policy_error() is not None
    safe_lan = manager.GameServerManager(
        "0.0.0.0", 0, "192.168.1.6", None, 0,
        auth_token="client-secret",
        allowed_clients=["192.168.1.0/24"],
    )
    assert safe_lan.control_plane_policy_error() is None
    assert safe_lan.is_authorized_request(
        {"action": "request_server", "authToken": "client-secret"}, "192.168.1.42"
    )
    assert safe_lan.is_authorized_request(
        {"action": "request_server", "authToken": "client-secret"}, "127.0.0.1"
    )
    assert not safe_lan.is_authorized_request(
        {"action": "request_server", "authToken": "client-secret"}, "10.0.0.42"
    )
    assert manager.parse_allowed_clients(["127.0.0.1", "192.168.1.0/24"])

    # Readiness must be authenticated and must match the configured level port;
    # an accepted message can only publish the manager-owned public endpoint.
    level_cfg = level_config["levels"][0]
    ready_manager = manager.GameServerManager(
        "127.0.0.1", 0, "127.0.0.1", None, 0,
        auth_token="client-secret",
        server_auth_token="server-secret",
    )
    ready_entry = manager.DedicatedServerEntry(level_cfg, 0)
    ready_entry.ready_nonce = "launch-nonce"
    ready_manager.levels[ready_entry.level_id] = ready_entry

    class FakeReader:
        def __init__(self, payload):
            self.payload = payload

        async def readline(self):
            return self.payload

    class FakeWriter:
        def __init__(self):
            self.responses = []
            self.closed = False

        def get_extra_info(self, name, default=None):
            return ("127.0.0.1", 42000) if name == "peername" else ("127.0.0.1", 0)

        def write(self, payload):
            self.responses.append(payload)

        async def drain(self):
            return None

        def close(self):
            self.closed = True

        async def wait_closed(self):
            return None

    valid_writer = FakeWriter()
    asyncio.run(
        ready_manager.handle_client(
            FakeReader(
                (json.dumps({
                    "action": "server_ready",
                    "levelId": ready_entry.level_id,
                    "port": ready_entry.port,
                    "serverAuthToken": "server-secret",
                    "readyNonce": "launch-nonce",
                    "host": "203.0.113.9",
                }) + "\n").encode("utf-8")
            ),
            valid_writer,
        )
    )
    assert ready_manager.ready_servers[ready_entry.level_id]["host"] == "127.0.0.1"
    assert ready_manager.ready_servers[ready_entry.level_id]["port"] == ready_entry.port

    invalid_writer = FakeWriter()
    asyncio.run(
        ready_manager.handle_client(
            FakeReader(
                (json.dumps({
                    "action": "server_ready",
                    "levelId": ready_entry.level_id,
                    "port": ready_entry.port + 1,
                    "serverAuthToken": "server-secret",
                    "readyNonce": "launch-nonce",
                }) + "\n").encode("utf-8")
            ),
            invalid_writer,
        )
    )
    assert ready_manager.ready_servers[ready_entry.level_id]["port"] == ready_entry.port
    assert any(b"Port does not match configured level" in response for response in invalid_writer.responses)

    captured = {}

    class FakeProcess:
        pid = 12345

        @staticmethod
        def poll():
            return None

    def fake_popen(args, **kwargs):
        captured["args"] = args
        captured["kwargs"] = kwargs
        return FakeProcess()

    original_popen = manager.subprocess.Popen
    manager.subprocess.Popen = fake_popen
    try:
        entry = manager.DedicatedServerEntry(
            {
                "id": "DebugGameLaunchContract",
                "mapPath": "/Game/Maps/StartupMap",
                "port": 17777,
                "queryPort": 27017,
                "launchArgs": ["-game", "-server"],
            },
            0,
            "NULL",
        )
        assert asyncio.run(
            entry._start(Path("UnrealEditor-Win64-DebugGame.exe"))
        )
    finally:
        manager.subprocess.Popen = original_popen

    assert captured["args"][:3] == [
        "UnrealEditor-Win64-DebugGame.exe",
        str(manager.PROJECT_FILE),
        "/Game/Maps/StartupMap?port=17777",
    ]
    assert "-AuraPersistenceProvider=NULL" in captured["args"]

    with tempfile.TemporaryDirectory() as temp_dir:
        engine_root = Path(temp_dir) / "Engine"
        editor_exe = engine_root / "Binaries" / "Win64" / "UnrealEditor-Win64-DebugGame.exe"
        editor_exe.parent.mkdir(parents=True)
        editor_exe.touch()
        (engine_root / "Build").mkdir()
        (engine_root / "Build" / "Build.version").write_text("{}", encoding="utf-8")
        gsm = manager.GameServerManager("127.0.0.1", 0, "127.0.0.1", None, 0)
        resolved = gsm._locate_editor_exe_for_client(
            {
                "clientExecutable": "UnrealEditor-Win64-DebugGame.exe",
                "clientEngineRoot": str(engine_root),
            }
        )
        assert resolved == editor_exe.resolve(), resolved

    # This checkout has a staged server from the final packaged validation. If
    # it is present, the manager must be able to recognize the cooked launch
    # path rather than selecting Binaries/Win64/AuraServer.exe.
    if staged_server.is_file():
        staged_root = staged_server.parent / "Aura" / "Content"
        assert staged_root.is_dir(), "Staged server content root is missing"
        assert any((staged_root / "Paks").glob("*.pak")) or any((staged_root / "Paks").glob("*.utoc"))

    function_names = {
        node.name
        for node in ast.walk(tree)
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef))
    }
    assert {"locate_server_exe", "locate_editor_exe"}.issubset(function_names)
    print("Game Server Manager cooked-server selection contract: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
