#!/usr/bin/env python3
"""
Aura Game Server Manager

Manages dedicated Unreal Engine server processes and routes clients
to the correct instance via TCP.

Protocol (newline-delimited JSON over TCP):
  Client -> Server: {"action": "request_server", "levelId": "<id>", "authToken": "<token>"}
  Dedicated server -> Server: {"action": "server_ready", "levelId": "<id>", "port": <port>, "serverAuthToken": "<token>", "readyNonce": "<nonce>"}
  Server -> Client: {"status": "ready", "host": "<host>", "port": <port>}
               or: {"status": "error", "message": "<reason>"}

Usage:
  python GameServerManager.py [--host 127.0.0.1] [--port 9000]
                               [--public-host 127.0.0.1]
                               [--auth-token <token>] [--server-auth-token <token>]
                               [--allowed-client <ip-or-cidr>]
                               [--server-exe /path/to/AuraServer]
                               [--persistence-provider NULL]
                               [--startup-grace 12]

Managed server processes also receive a per-launch readiness nonce through
AURA_GSM_SERVER_READY_NONCE; it is never logged and must be echoed only by that
process. Non-loopback binds require AURA_GSM_AUTH_TOKEN, AURA_GSM_SERVER_AUTH_TOKEN (or
the request token as a shared token), and at least one AURA_GSM_ALLOWED_CLIENTS
entry. AURA_GSM_ADDRESS is the client/server connect endpoint when it differs
from the bind address. Tokens are read from the environment, inherited by
managed servers, and never logged.
"""

import argparse
import asyncio
import hmac
import ipaddress
import json
import logging
import os
import secrets
import subprocess
import sys
import time
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Union

# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)
logger = logging.getLogger("AuraGSM")

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_DIR = SCRIPT_DIR.parent
PROJECT_FILE = PROJECT_DIR / "Aura.uproject"
LEVEL_CONFIG_PATH = PROJECT_DIR / "Content" / "Config" / "LevelConfig.json"
GSM_LOG_DIR = PROJECT_DIR / "Saved" / "Logs" / "GameServerManager"

# Maximum bytes accepted in a single client request line (prevents abuse).
MAX_REQUEST_BYTES = 4096
# The game client abandons a manager query after 35 seconds. Leave time for
# the explicit error to reach it instead of triggering transport fallback.
SERVER_REQUEST_BUDGET_SECONDS = 30.0
AUTH_TOKEN_ENV = "AURA_GSM_AUTH_TOKEN"
SERVER_AUTH_TOKEN_ENV = "AURA_GSM_SERVER_AUTH_TOKEN"
ALLOWED_CLIENTS_ENV = "AURA_GSM_ALLOWED_CLIENTS"
READY_NONCE_ENV = "AURA_GSM_SERVER_READY_NONCE"
EDITOR_EXECUTABLE_NAMES = frozenset(
    {
        "unrealeditor.exe",
        "unrealeditor-cmd.exe",
        "unrealeditor-win64-debuggame.exe",
        "unrealeditor-win64-development.exe",
        "unrealeditor-win64-shipping.exe",
        "unrealeditor-win64-test.exe",
    }
)


def is_editor_executable(executable: Path) -> bool:
    """Return whether *executable* is an Unreal Editor runtime variant."""
    return executable.name.lower() in EDITOR_EXECUTABLE_NAMES


def editor_executable_name(client_executable: str) -> str:
    """Keep the client/editor target variant when resolving the server runtime.

    Unreal reports different executable names for Development, DebugGame, and
    command-line editor clients.  Falling back to the generic executable for a
    known editor variant can select a different target receipt and module set.
    """
    raw_name = str(client_executable or "").strip().replace("\\", "/").rsplit("/", 1)[-1]
    name = Path(raw_name).name
    if name.lower() in EDITOR_EXECUTABLE_NAMES:
        return name
    return "UnrealEditor.exe"


def is_loopback_host(host: str) -> bool:
    """Return whether a bind address is explicitly loopback-only."""
    normalized = str(host or "").strip().lower()
    if normalized == "localhost":
        return True
    try:
        return ipaddress.ip_address(normalized).is_loopback
    except ValueError:
        # Hostnames are not treated as loopback unless they are the explicit
        # localhost name; an unresolved/ambiguous name must opt into policy.
        return False


def parse_allowed_clients(values: Optional[Sequence[str]]) -> List[Union[ipaddress.IPv4Network, ipaddress.IPv6Network]]:
    """Parse individual IPs or CIDR networks used by the control plane."""
    networks: List[Union[ipaddress.IPv4Network, ipaddress.IPv6Network]] = []
    for raw_value in values or []:
        for raw_item in str(raw_value).split(","):
            item = raw_item.strip()
            if not item:
                continue
            try:
                if "/" in item:
                    networks.append(ipaddress.ip_network(item, strict=False))
                else:
                    address = ipaddress.ip_address(item)
                    networks.append(ipaddress.ip_network(f"{address}/{address.max_prefixlen}"))
            except ValueError as exc:
                raise ValueError(f"Invalid allowed client address '{item}'") from exc
    return networks

# ---------------------------------------------------------------------------
# Level / server entry
# ---------------------------------------------------------------------------

class DedicatedServerEntry:
    """Tracks a single dedicated server process for one level."""

    def __init__(
        self,
        level_cfg: dict,
        startup_grace: float,
        persistence_provider: Optional[str] = None,
    ):
        self.level_id: str = level_cfg["id"]
        self.display_name: str = level_cfg.get("displayName", self.level_id)
        self.map_path: str = level_cfg["mapPath"]
        self.port: int = int(level_cfg["port"])
        self.query_port: int = int(level_cfg.get("queryPort", 0))
        self.launch_args: List[str] = level_cfg.get(
            "launchArgs", ["-game", "-server", "-log", "-unattended", "-NoLiveCoding"]
        )
        self.startup_grace: float = startup_grace
        self.persistence_provider: str = str(persistence_provider or "").strip()

        self._process: Optional[subprocess.Popen] = None
        self._server_exe: Optional[Path] = None
        self.ready_nonce: Optional[str] = None
        # asyncio.Lock serialises start requests for this level.
        self._start_lock: Optional[asyncio.Lock] = None

    def _ensure_lock(self) -> asyncio.Lock:
        if self._start_lock is None:
            self._start_lock = asyncio.Lock()
        return self._start_lock

    # ------------------------------------------------------------------
    def is_running(self) -> bool:
        if self._process is None:
            return False
        return self._process.poll() is None

    def get_pid(self) -> Optional[int]:
        if self._process is None:
            return None
        return self._process.pid

    def get_server_exe(self) -> Optional[Path]:
        return self._server_exe

    # ------------------------------------------------------------------
    async def ensure_running(self, server_exe: Optional[Path]) -> bool:
        """
        Ensure the dedicated server is running.
        If it is already up, return True immediately.
        If not, start it and wait for the startup grace period.
        Returns False if starting fails.
        """
        async with self._ensure_lock():
            if self.is_running():
                running_exe = self.get_server_exe()
                if running_exe is not None and server_exe is not None:
                    try:
                        same_executable = running_exe.resolve() == server_exe.resolve()
                    except OSError:
                        same_executable = running_exe == server_exe
                    if not same_executable:
                        logger.info(
                            "DS '%s' is running with '%s'; restarting it with '%s' for client compatibility",
                            self.level_id,
                            running_exe,
                            server_exe,
                        )
                        self.stop()
                    else:
                        logger.info(
                            "DS '%s' already running (pid=%s, port=%d)",
                            self.level_id,
                            str(self.get_pid()),
                            self.port,
                        )
                        return True
                else:
                    logger.info(
                        "DS '%s' already running (pid=%s, port=%d)",
                        self.level_id,
                        str(self.get_pid()),
                        self.port,
                    )
                    return True

            if server_exe is None:
                logger.error(
                    "Cannot start level '%s': server executable not found", self.level_id
                )
                return False

            return await self._start(server_exe)

    async def _start(self, server_exe: Path) -> bool:
        start_t = time.monotonic()
        map_arg = f"{self.map_path}?port={self.port}"
        GSM_LOG_DIR.mkdir(parents=True, exist_ok=True)
        log_file = GSM_LOG_DIR / f"{self.level_id}.log"

        if is_editor_executable(server_exe):
            args = [str(server_exe), str(PROJECT_FILE), map_arg] + list(self.launch_args)
            launch_mode = "editor"
        else:
            args = [str(server_exe), map_arg] + list(self.launch_args)
            launch_mode = "server"

        args.append(f"-port={self.port}")
        if self.query_port > 0:
            args.append(f"-QueryPort={self.query_port}")
        args.append(f"-Abslog={log_file}")
        if self.persistence_provider:
            args.append(f"-AuraPersistenceProvider={self.persistence_provider}")

        logger.info("Starting DS '%s' via %s launcher: %s", self.level_id, launch_mode, " ".join(args))
        logger.info("DS '%s' log file: %s", self.level_id, log_file)
        try:
            popen_kwargs = {}
            ready_nonce = secrets.token_urlsafe(32)
            child_environment = os.environ.copy()
            child_environment[READY_NONCE_ENV] = ready_nonce
            popen_kwargs["env"] = child_environment
            if os.name == "nt":
                # Ensure Unreal's -log output is visible in its own console window.
                popen_kwargs["creationflags"] = subprocess.CREATE_NEW_CONSOLE

            self._process = subprocess.Popen(args, **popen_kwargs)
            self._server_exe = server_exe.resolve()
            self.ready_nonce = ready_nonce
        except Exception as exc:
            logger.error("Failed to launch DS '%s': %s", self.level_id, exc)
            self._process = None
            self._server_exe = None
            self.ready_nonce = None
            return False

        logger.info(
            "DS '%s' started (PID=%d); waiting %.1fs for startup",
            self.level_id,
            self._process.pid,
            self.startup_grace,
        )
        await asyncio.sleep(self.startup_grace)

        if not self.is_running():
            rc = self._process.poll() if self._process is not None else None
            logger.error(
                "DS '%s' exited prematurely during startup (returncode=%s, elapsed=%.3fs)",
                self.level_id,
                str(rc),
                time.monotonic() - start_t,
            )
            return False

        logger.info(
            "DS '%s' process is alive on port %d; world readiness still requires server_ready (pid=%d, elapsed=%.3fs)",
            self.level_id,
            self.port,
            self._process.pid,
            time.monotonic() - start_t,
        )
        return True

    def stop(self) -> None:
        if self._process is not None and self.is_running():
            logger.info("Stopping DS '%s' (PID=%d)", self.level_id, self._process.pid)
            self._process.terminate()
            try:
                self._process.wait(timeout=5)
                logger.info("DS '%s' stopped gracefully (PID=%d)", self.level_id, self._process.pid)
            except subprocess.TimeoutExpired:
                logger.warning("DS '%s' did not stop in time; killing (PID=%d)", self.level_id, self._process.pid)
                self._process.kill()
                logger.info("DS '%s' killed (PID=%d)", self.level_id, self._process.pid)
        self._process = None
        self._server_exe = None
        self.ready_nonce = None


# ---------------------------------------------------------------------------
# Manager
# ---------------------------------------------------------------------------

class GameServerManager:
    def __init__(
        self,
        listen_host: str,
        listen_port: int,
        public_host: str,
        server_exe_override: Optional[str],
        startup_grace: float,
        persistence_provider_override: Optional[str] = None,
        auth_token: Optional[str] = None,
        server_auth_token: Optional[str] = None,
        allowed_clients: Optional[Sequence[str]] = None,
    ):
        self.listen_host = listen_host
        self.listen_port = listen_port
        self.public_host = public_host
        self.server_exe_override = server_exe_override
        self.startup_grace = startup_grace
        self.persistence_provider_override = str(persistence_provider_override or "").strip()
        self.auth_token = str(
            auth_token if auth_token is not None else os.environ.get(AUTH_TOKEN_ENV, "")
        ).strip()
        self.server_auth_token = str(
            server_auth_token
            if server_auth_token is not None
            else os.environ.get(SERVER_AUTH_TOKEN_ENV, "")
        ).strip() or self.auth_token
        configured_clients = (
            allowed_clients
            if allowed_clients is not None
            else [os.environ.get(ALLOWED_CLIENTS_ENV, "")]
        )
        self.allowed_clients = parse_allowed_clients(configured_clients)

        self.levels: Dict[str, DedicatedServerEntry] = {}
        self.server_exe: Optional[Path] = None
        self.request_counter = 0
        self.ready_servers: Dict[str, dict] = {}
        self.ready_events: Dict[str, asyncio.Event] = {}

    def control_plane_policy_error(self) -> Optional[str]:
        """Return the startup policy violation, if this bind is unsafe."""
        if is_loopback_host(self.listen_host):
            return None
        if not self.auth_token:
            return (
                f"non-loopback GSM bind '{self.listen_host}' requires "
                f"{AUTH_TOKEN_ENV}"
            )
        if not self.server_auth_token:
            return (
                f"non-loopback GSM bind '{self.listen_host}' requires "
                f"{SERVER_AUTH_TOKEN_ENV} (or {AUTH_TOKEN_ENV})"
            )
        if not self.allowed_clients:
            return (
                f"non-loopback GSM bind '{self.listen_host}' requires a non-empty "
                f"{ALLOWED_CLIENTS_ENV} allowlist or --allowed-client"
            )
        return None

    @staticmethod
    def _peer_ip(peer_host: object) -> Optional[Union[ipaddress.IPv4Address, ipaddress.IPv6Address]]:
        try:
            address = ipaddress.ip_address(str(peer_host))
        except ValueError:
            return None
        return getattr(address, "ipv4_mapped", None) or address

    def _peer_is_allowed(self, peer_host: object) -> bool:
        address = self._peer_ip(peer_host)
        if address is None:
            return False
        # A managed server may connect through loopback even when the manager
        # itself is also bound to a LAN address. Authentication is still
        # required for non-loopback binds, so this does not create an open path.
        if address.is_loopback:
            return True
        if self.allowed_clients:
            return any(address in network for network in self.allowed_clients)
        return False

    def _expected_token(self, action: str) -> str:
        return self.server_auth_token if action == "server_ready" else self.auth_token

    def is_authorized_request(self, request: object, peer_host: object) -> bool:
        """Authenticate a control-plane message before any state mutation."""
        if not isinstance(request, dict) or not self._peer_is_allowed(peer_host):
            return False

        action = str(request.get("action", "")).strip()
        expected = self._expected_token(action)
        if not expected:
            return is_loopback_host(self.listen_host)

        supplied_field = "serverAuthToken" if action == "server_ready" else "authToken"
        supplied = request.get(supplied_field)
        return isinstance(supplied, str) and hmac.compare_digest(supplied, expected)

    def _next_request_id(self) -> str:
        self.request_counter += 1
        return f"req-{self.request_counter:06d}"

    def _get_ready_event(self, level_id: str) -> asyncio.Event:
        event = self.ready_events.get(level_id)
        if event is None:
            event = asyncio.Event()
            self.ready_events[level_id] = event
        return event

    def _clear_ready_state(self, level_id: str) -> None:
        self.ready_servers.pop(level_id, None)
        self._get_ready_event(level_id).clear()

    async def _wait_for_ready(self, level_id: str, expected_port: int, timeout_seconds: float) -> Optional[dict]:
        existing = self.ready_servers.get(level_id)
        if existing and int(existing.get("port", 0)) == expected_port:
            return existing

        event = self._get_ready_event(level_id)
        try:
            await asyncio.wait_for(event.wait(), timeout=timeout_seconds)
        except asyncio.TimeoutError:
            return None

        ready = self.ready_servers.get(level_id)
        if not ready:
            return None
        if int(ready.get("port", 0)) != expected_port:
            return None
        return ready

    # ------------------------------------------------------------------
    def load_levels(self) -> None:
        try:
            with open(LEVEL_CONFIG_PATH, "r", encoding="utf-8") as fh:
                config = json.load(fh)
        except Exception as exc:
            logger.error("Failed to read LevelConfig.json at %s: %s", LEVEL_CONFIG_PATH, exc)
            sys.exit(1)

        for entry_cfg in config.get("levels", []):
            if "id" not in entry_cfg:
                logger.warning(
                    "Level entry missing 'id', skipping: %s",
                    entry_cfg.get("displayName", "?"),
                )
                continue
            if "port" not in entry_cfg:
                logger.warning(
                    "Level '%s' missing 'port', skipping", entry_cfg.get("id", "?")
                )
                continue

            entry = DedicatedServerEntry(
                entry_cfg,
                self.startup_grace,
                self.persistence_provider_override,
            )
            self.levels[entry.level_id] = entry
            logger.info(
                "Registered level: id='%s'  name='%s'  port=%d",
                entry.level_id,
                entry.display_name,
                entry.port,
            )

        logger.info("Loaded %d level(s) from LevelConfig.json", len(self.levels))

    # ------------------------------------------------------------------
    def locate_server_exe(self) -> Optional[Path]:
        if self.server_exe_override:
            p = Path(self.server_exe_override)
            if p.exists():
                logger.info("Using server executable from override: %s", p)
                return p
            logger.warning("AURA_SERVER_EXE / --server-exe not found at: %s", p)

        env_val = os.environ.get("AURA_SERVER_EXE")
        if env_val:
            p = Path(env_val)
            if p.exists():
                logger.info("Using server executable from env: %s", p)
                return p

        # A non-editor Development server cannot load loose .umap files from
        # Content. It needs the cooked payload beside the launcher. Prefer the
        # current staged server and then the newest archived server package.
        cooked_candidates: List[Path] = [
            PROJECT_DIR / "Saved" / "StagedBuilds" / "WindowsServer" / "AuraServer.exe",
        ]
        saved_dir = PROJECT_DIR / "Saved"
        if saved_dir.is_dir():
            cooked_candidates.extend(
                package_dir / "WindowsServer" / "AuraServer.exe"
                for package_dir in sorted(
                    (path for path in saved_dir.glob("*") if path.is_dir()),
                    key=lambda path: path.stat().st_mtime,
                    reverse=True,
                )
                if (package_dir / "WindowsServer" / "AuraServer.exe").exists()
            )

        seen_candidates = set()
        for candidate in cooked_candidates:
            candidate = candidate.resolve()
            if candidate in seen_candidates or not candidate.exists():
                continue
            seen_candidates.add(candidate)
            payload_roots = (
                candidate.parent / "Aura" / "Content",
                candidate.parent / "Content",
            )
            has_cooked_payload = any(
                content_root.is_dir()
                and (
                    (content_root / "Maps" / "StartupMap.umap").exists()
                    or any((content_root / "Paks").glob("*.pak"))
                    or any((content_root / "Paks").glob("*.utoc"))
                )
                for content_root in payload_roots
            )
            if has_cooked_payload:
                logger.info("Found cooked server executable: %s", candidate)
                return candidate

        uncooked_candidates = [
            PROJECT_DIR / "Binaries" / "Win64" / "AuraServer.exe",
            PROJECT_DIR / "Binaries" / "Mac" / "AuraServer",
            PROJECT_DIR / "Binaries" / "Mac" / "Aura-Mac-Shipping",
        ]
        if any(candidate.exists() for candidate in uncooked_candidates):
            logger.warning(
                "Ignoring uncooked AuraServer binary under Binaries; it cannot load "
                "StartupMap. Stage/cook a server package or use the editor server fallback."
            )

        editor_exe = self.locate_editor_exe()
        if editor_exe is not None:
            logger.warning(
                "Using UnrealEditor server fallback because no cooked AuraServer package was found: %s",
                editor_exe,
            )
            return editor_exe

        logger.warning(
            "No cooked server executable found. Run BuildCookRun to stage a server package, "
            "set AURA_SERVER_EXE/--server-exe to one, or set UE_EDITOR_EXE for the editor fallback."
        )
        return None

    # ------------------------------------------------------------------
    def locate_editor_exe(self) -> Optional[Path]:
        env_val = os.environ.get("UE_EDITOR_EXE")
        if env_val:
            p = Path(env_val)
            if p.exists():
                logger.info("Using UnrealEditor from UE_EDITOR_EXE: %s", p)
                return p
            logger.warning("UE_EDITOR_EXE points to a missing file: %s", p)

        if os.name != "nt":
            return None

        engine_assoc = ""
        try:
            if PROJECT_FILE.exists():
                project_json = json.loads(PROJECT_FILE.read_text(encoding="utf-8"))
                engine_assoc = str(project_json.get("EngineAssociation", "")).strip()
        except Exception as exc:
            logger.warning("Failed reading EngineAssociation from %s: %s", PROJECT_FILE, exc)

        def query_reg_installed_dir(key: str, value_name: str) -> Optional[str]:
            try:
                result = subprocess.run(
                    ["reg", "query", key, "/v", value_name],
                    capture_output=True,
                    text=True,
                    check=False,
                )
            except Exception:
                return None

            if result.returncode != 0 or not result.stdout:
                return None

            for line in result.stdout.splitlines():
                line = line.strip()
                if value_name.lower() not in line.lower():
                    continue

                parts = line.split()
                if len(parts) < 3:
                    continue

                # Registry output: <Name>    <Type>    <Data with spaces>
                data = " ".join(parts[2:]).strip()
                if data:
                    return data
            return None

        candidate_engine_dirs: List[str] = []
        if engine_assoc:
            maybe = query_reg_installed_dir(
                rf"HKLM\SOFTWARE\EpicGames\Unreal Engine\{engine_assoc}",
                "InstalledDirectory",
            )
            if maybe:
                candidate_engine_dirs.append(maybe)

            maybe = query_reg_installed_dir(
                rf"HKLM\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\{engine_assoc}",
                "InstalledDirectory",
            )
            if maybe:
                candidate_engine_dirs.append(maybe)

            maybe = query_reg_installed_dir(
                r"HKCU\SOFTWARE\Epic Games\Unreal Engine\Builds",
                engine_assoc,
            )
            if maybe:
                candidate_engine_dirs.append(maybe)

        for engine_dir in candidate_engine_dirs:
            candidate = Path(engine_dir) / "Engine" / "Binaries" / "Win64" / "UnrealEditor.exe"
            if candidate.exists():
                logger.info("Found UnrealEditor executable from registry: %s", candidate)
                return candidate

        logger.warning("Could not auto-discover UnrealEditor.exe; set UE_EDITOR_EXE explicitly")
        return None

    @staticmethod
    def _is_editor_client(request: dict) -> bool:
        executable = str(request.get("clientExecutable", "")).strip().lower()
        return executable.startswith("unrealeditor")

    def _locate_editor_exe_for_client(self, request: dict) -> Optional[Path]:
        """Resolve the editor executable that produced this client request.

        An UnrealEditor client and a packaged AuraServer do not share the same
        network changelist (the editor carries the source engine CL while a
        packaged target commonly reports NetCL=0).  For local development, use
        the same engine root reported by the editor client so its server side
        has the identical network fingerprint.
        """
        engine_root = str(request.get("clientEngineRoot", "")).strip()
        if engine_root:
            try:
                root = Path(engine_root).expanduser().resolve()
                executable_name = editor_executable_name(request.get("clientExecutable", ""))
                if root.name.lower() == "engine":
                    candidate = root / "Binaries" / "Win64" / executable_name
                else:
                    candidate = root / "Engine" / "Binaries" / "Win64" / executable_name
                build_version = root / "Build" / "Build.version" if root.name.lower() == "engine" else root / "Engine" / "Build" / "Build.version"
                if candidate.is_file() and build_version.is_file():
                    logger.info(
                        "Using matching UnrealEditor runtime reported by client: %s",
                        candidate,
                    )
                    return candidate
                logger.warning(
                    "Client reported an invalid Unreal Engine root or missing runtime "
                    "'%s': %s",
                    executable_name,
                    engine_root,
                )
            except OSError as exc:
                logger.warning("Could not resolve client Unreal Engine root '%s': %s", engine_root, exc)

        return self.locate_editor_exe()

    # ------------------------------------------------------------------
    def _locate_build_bat(self) -> Optional[Path]:
        """
        Locate UBT Build.bat using UE_ENGINE_ROOT env var first, then registry
        via the project's EngineAssociation — mirrors the logic in BuildDedicatedServer.bat.
        """
        env_root = os.environ.get("UE_ENGINE_ROOT")
        if env_root:
            candidate = Path(env_root) / "Engine" / "Build" / "BatchFiles" / "Build.bat"
            if candidate.exists():
                logger.info("Found Build.bat via UE_ENGINE_ROOT: %s", candidate)
                return candidate
            logger.warning("UE_ENGINE_ROOT set but Build.bat not found at: %s", candidate)

        # Read EngineAssociation from the .uproject file.
        engine_assoc = ""
        try:
            if PROJECT_FILE.exists():
                project_json = json.loads(PROJECT_FILE.read_text(encoding="utf-8"))
                engine_assoc = str(project_json.get("EngineAssociation", "")).strip()
        except Exception as exc:
            logger.warning("Failed reading EngineAssociation from %s: %s", PROJECT_FILE, exc)

        if not engine_assoc:
            logger.warning(
                "Cannot locate Build.bat: EngineAssociation is empty and UE_ENGINE_ROOT is not set."
            )
            return None

        def _query_reg(key: str, value_name: str) -> Optional[str]:
            try:
                result = subprocess.run(
                    ["reg", "query", key, "/v", value_name],
                    capture_output=True,
                    text=True,
                    check=False,
                )
            except Exception:
                return None
            if result.returncode != 0 or not result.stdout:
                return None
            for line in result.stdout.splitlines():
                line = line.strip()
                if value_name.lower() not in line.lower():
                    continue
                parts = line.split()
                if len(parts) >= 3:
                    return " ".join(parts[2:]).strip()
            return None

        candidate_engine_dirs = [
            _query_reg(
                rf"HKLM\SOFTWARE\EpicGames\Unreal Engine\{engine_assoc}",
                "InstalledDirectory",
            ),
            _query_reg(
                rf"HKLM\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\{engine_assoc}",
                "InstalledDirectory",
            ),
            _query_reg(
                r"HKCU\SOFTWARE\Epic Games\Unreal Engine\Builds",
                engine_assoc,
            ),
        ]
        for engine_dir in candidate_engine_dirs:
            if not engine_dir:
                continue
            candidate = Path(engine_dir) / "Engine" / "Build" / "BatchFiles" / "Build.bat"
            if candidate.exists():
                logger.info("Found Build.bat via registry (assoc=%s): %s", engine_assoc, candidate)
                return candidate

        logger.warning(
            "Could not find Build.bat for EngineAssociation '%s'. "
            "Set UE_ENGINE_ROOT to your engine root directory.",
            engine_assoc,
        )
        return None

    # ------------------------------------------------------------------
    async def _build_server_binary(self, request_id: str) -> bool:
        """
        Run UBT to build AuraServer Win64 Development.
        Streams build output to the logger line by line.
        Returns True on success, False on any failure.
        """
        build_bat = self._locate_build_bat()
        if build_bat is None:
            logger.error(
                "[%s] Cannot build AuraServer: Build.bat not found. "
                "Set UE_ENGINE_ROOT or ensure EngineAssociation is valid.",
                request_id,
            )
            return False

        cmd = [
            "cmd", "/c", str(build_bat),
            "AuraServer", "Win64", "Development",
            str(PROJECT_FILE),
            "-waitmutex",
        ]
        logger.info("[%s] Building AuraServer binary: %s", request_id, " ".join(cmd))
        build_start = time.monotonic()

        try:
            proc = await asyncio.create_subprocess_exec(
                *cmd,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.STDOUT,
            )
        except Exception as exc:
            logger.error("[%s] Failed to launch build process: %s", request_id, exc)
            return False

        # Stream UBT output line-by-line so progress is visible in the GSM log.
        assert proc.stdout is not None
        while True:
            line = await proc.stdout.readline()
            if not line:
                break
            logger.info("[%s] [BUILD] %s", request_id, line.decode("utf-8", errors="replace").rstrip())

        rc = await proc.wait()
        elapsed = time.monotonic() - build_start
        if rc == 0:
            logger.info("[%s] AuraServer build succeeded (elapsed=%.1fs)", request_id, elapsed)
            return True

        logger.error(
            "[%s] AuraServer build FAILED (returncode=%d, elapsed=%.1fs). "
            "If the error says 'Server targets are not currently supported', you need a "
            "source-built Unreal Engine and must set UE_ENGINE_ROOT.",
            request_id, rc, elapsed,
        )
        return False

    # ------------------------------------------------------------------
    async def handle_client(
        self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter
    ) -> None:
        peer = writer.get_extra_info("peername", ("?", 0))
        local = writer.get_extra_info("sockname", ("?", 0))
        request_id = self._next_request_id()
        request_start = time.monotonic()

        logger.info(
            "[%s] Client connected remote=%s:%s local=%s:%s",
            request_id,
            peer[0],
            peer[1],
            local[0],
            local[1],
        )

        try:
            try:
                raw = await asyncio.wait_for(
                    reader.readline(), timeout=10.0
                )
            except asyncio.TimeoutError:
                logger.warning("[%s] Timed out waiting for request line from %s:%s", request_id, peer[0], peer[1])
                await self._send_error(writer, request_id, "Request timeout")
                return
            except Exception as exc:
                logger.warning("[%s] Read error from %s:%s: %s", request_id, peer[0], peer[1], exc)
                return

            if not raw or len(raw) > MAX_REQUEST_BYTES:
                logger.warning(
                    "[%s] Invalid request size from %s:%s (bytes=%d, max=%d)",
                    request_id,
                    peer[0],
                    peer[1],
                    len(raw) if raw is not None else 0,
                    MAX_REQUEST_BYTES,
                )
                await self._send_error(writer, request_id, "Request too large or empty")
                return

            logger.info("[%s] Received request line bytes=%d", request_id, len(raw))

            # Parse JSON
            try:
                decoded = raw.decode("utf-8").strip()
                request = json.loads(decoded)
            except json.JSONDecodeError as exc:
                logger.warning(
                    "[%s] Bad JSON from %s:%s: %s | payload=%r",
                    request_id,
                    peer[0],
                    peer[1],
                    exc,
                    raw[:256],
                )
                await self._send_error(writer, request_id, "Invalid JSON")
                return
            except UnicodeDecodeError as exc:
                logger.warning(
                    "[%s] Non-UTF8 payload from %s:%s: %s",
                    request_id,
                    peer[0],
                    peer[1],
                    exc,
                )
                await self._send_error(writer, request_id, "Request must be UTF-8 JSON")
                return

            if not isinstance(request, dict):
                await self._send_error(writer, request_id, "Request must be a JSON object")
                return

            action = request.get("action", "")
            if action in {"server_ready", "request_server"} and not self.is_authorized_request(request, peer[0]):
                logger.warning(
                    "[%s] Unauthorized GSM action=%s from %s:%s",
                    request_id,
                    action,
                    peer[0],
                    peer[1],
                )
                await self._send_error(writer, request_id, "Unauthorized control-plane request")
                return

            if action == "server_ready":
                level_id = str(request.get("levelId", "")).strip()
                if not level_id:
                    logger.warning("[%s] server_ready missing levelId from %s:%s", request_id, peer[0], peer[1])
                    await self._send_error(writer, request_id, "Missing levelId")
                    return

                try:
                    ready_port = int(request.get("port", 0))
                except (TypeError, ValueError):
                    ready_port = 0

                if ready_port < 1 or ready_port > 65535:
                    logger.warning("[%s] server_ready invalid port=%r for levelId=%s", request_id, request.get("port"), level_id)
                    await self._send_error(writer, request_id, "Invalid port")
                    return

                entry = self.levels.get(level_id)
                if entry is None:
                    logger.warning(
                        "[%s] server_ready unknown levelId=%s from %s:%s",
                        request_id,
                        level_id,
                        peer[0],
                        peer[1],
                    )
                    await self._send_error(writer, request_id, f"Unknown levelId: {level_id!r}")
                    return
                if ready_port != entry.port:
                    logger.warning(
                        "[%s] server_ready port mismatch levelId=%s announced=%d configured=%d",
                        request_id,
                        level_id,
                        ready_port,
                        entry.port,
                    )
                    await self._send_error(writer, request_id, "Port does not match configured level")
                    return

                expected_nonce = entry.ready_nonce
                supplied_nonce = request.get("readyNonce")
                if (
                    not expected_nonce
                    or not isinstance(supplied_nonce, str)
                    or not hmac.compare_digest(supplied_nonce, expected_nonce)
                ):
                    logger.warning(
                        "[%s] server_ready readiness nonce mismatch levelId=%s",
                        request_id,
                        level_id,
                    )
                    await self._send_error(writer, request_id, "Invalid readiness nonce")
                    return

                # The endpoint returned to clients is manager configuration, not
                # caller-controlled input. This prevents a malformed readiness
                # message from redirecting clients.
                announced_host = self.public_host
                self.ready_servers[level_id] = {
                    "host": announced_host,
                    "port": entry.port,
                    "timestamp": time.time(),
                }
                self._get_ready_event(level_id).set()

                logger.info(
                    "[%s] Registered server_ready levelId=%s host=%s port=%d",
                    request_id,
                    level_id,
                    announced_host,
                    ready_port,
                )
                writer.write((json.dumps({"status": "ok", "message": "registered"}) + "\n").encode("utf-8"))
                await writer.drain()
                logger.info("[%s] server_ready ack sent", request_id)
                return

            if action != "request_server":
                logger.warning("[%s] Unsupported action from %s:%s: %r", request_id, peer[0], peer[1], action)
                await self._send_error(writer, request_id, f"Unknown action: {action!r}")
                return

            level_id = str(request.get("levelId", "")).strip()
            if not level_id:
                logger.warning("[%s] Missing levelId in request from %s:%s", request_id, peer[0], peer[1])
                await self._send_error(writer, request_id, "Missing levelId")
                return

            entry = self.levels.get(level_id)
            if entry is None:
                logger.warning("[%s] Unknown levelId '%s' from %s:%s", request_id, level_id, peer[0], peer[1])
                await self._send_error(writer, request_id, f"Unknown levelId: {level_id!r}")
                return

            logger.info(
                "[%s] Request accepted remote=%s:%s levelId='%s' clientExecutable='%s' clientEngineRoot='%s' clientNetworkChangelist=%s clientNetworkVersion=%s",
                request_id,
                peer[0],
                peer[1],
                level_id,
                str(request.get("clientExecutable", "")),
                str(request.get("clientEngineRoot", "")),
                str(request.get("clientNetworkChangelist", "")),
                str(request.get("clientNetworkVersion", "")),
            )

            was_running_before = entry.is_running()
            editor_client = self._is_editor_client(request)
            requested_server_exe = (
                self._locate_editor_exe_for_client(request)
                if editor_client
                else self.server_exe
            )
            if editor_client and requested_server_exe is None:
                await self._send_error(
                    writer,
                    request_id,
                    "This UnrealEditor client requires a matching UnrealEditor server. "
                    "Set UE_EDITOR_EXE or send the request from the configured engine checkout.",
                )
                return

            running_server_exe = entry.get_server_exe()
            executable_changed = (
                entry.is_running()
                and running_server_exe is not None
                and requested_server_exe is not None
                and running_server_exe.resolve() != requested_server_exe.resolve()
            )
            if executable_changed:
                logger.info(
                    "[%s] Restarting levelId='%s' to match %s client runtime: '%s' -> '%s'",
                    request_id,
                    level_id,
                    "editor" if editor_client else "packaged",
                    running_server_exe,
                    requested_server_exe,
                )
                entry.stop()
                self._clear_ready_state(level_id)
                was_running_before = False
            if not was_running_before:
                self._clear_ready_state(level_id)
                logger.info("[%s] Cleared stale ready state for levelId='%s' before launch", request_id, level_id)

                # Resolve the packaged server binary before launching. The manager can
                # stay alive while a first build creates AuraServer.exe; keeping
                # self.server_exe=None in that case used to force every first
                # request through a blocking UBT build and exceed the client's
                # 35-second query budget even though the binary was already ready.
                if not editor_client and (self.server_exe is None or not self.server_exe.exists()):
                    self.server_exe = self.locate_server_exe()

                is_editor_mode = (
                    editor_client
                    or (self.server_exe is not None and is_editor_executable(self.server_exe))
                )
                if requested_server_exe is None and not is_editor_mode:
                    logger.info(
                        "[%s] Server not running — building AuraServer binary before launch (levelId='%s')",
                        request_id, level_id,
                    )
                    build_ok = await self._build_server_binary(request_id)
                    if not build_ok:
                        await self._send_error(
                            writer,
                            request_id,
                            f"Dedicated server binary build failed for '{level_id}'. "
                            "Check the GSM log for UBT output.",
                        )
                        return
                    # Re-locate the executable in case the build just produced it
                    # (e.g. the binary was absent at GSM startup).
                    if self.server_exe is None:
                        self.server_exe = self.locate_server_exe()
                    requested_server_exe = self.server_exe

                if requested_server_exe is None:
                    await self._send_error(
                        writer,
                        request_id,
                        f"No server executable is available for '{level_id}'.",
                    )
                    return

            remaining = max(0.0, SERVER_REQUEST_BUDGET_SECONDS - (time.monotonic() - request_start))
            try:
                ok = await asyncio.wait_for(entry.ensure_running(requested_server_exe), timeout=remaining)
            except asyncio.TimeoutError:
                await self._send_error(writer, request_id,
                    f"Dedicated server for '{level_id}' exceeded the startup request deadline. Retry shortly; check the server log if it persists.")
                return
            if not ok:
                logger.error("[%s] Failed ensuring DS is running for levelId='%s'", request_id, level_id)
                await self._send_error(
                    writer,
                    request_id,
                    f"Dedicated server for '{level_id}' could not be started. "
                    "Please start it manually and retry.",
                )
                return

            remaining = max(0.0, SERVER_REQUEST_BUDGET_SECONDS - (time.monotonic() - request_start))
            ready_info = await self._wait_for_ready(level_id, entry.port, remaining)
            if ready_info is None:
                logger.error(
                    "[%s] Timed out waiting for server_ready for levelId='%s' port=%d",
                    request_id,
                    level_id,
                    entry.port,
                )
                await self._send_error(
                    writer,
                    request_id,
                    f"Dedicated server for '{level_id}' did not report healthy world readiness before the request deadline. Check its server log for WorldReadiness or persistence errors.",
                )
                return

            response = {
                "status": "ready",
                "host": str(ready_info.get("host", self.public_host)),
                "port": int(ready_info.get("port", entry.port)),
                "serverMode": (
                    "editor"
                    if requested_server_exe is not None and is_editor_executable(requested_server_exe)
                    else "packaged"
                ),
            }
            logger.info(
                "[%s] Responding ready remote=%s:%s host=%s port=%d ds_pid=%s elapsed=%.3fs",
                request_id,
                peer[0],
                peer[1],
                response["host"],
                response["port"],
                str(entry.get_pid()),
                time.monotonic() - request_start,
            )
            writer.write((json.dumps(response) + "\n").encode("utf-8"))
            try:
                await writer.drain()
                logger.info("[%s] Response flushed successfully", request_id)
            except Exception as exc:
                logger.warning("[%s] Failed while flushing response: %s", request_id, exc)

        finally:
            logger.info("[%s] Request complete total_elapsed=%.3fs", request_id, time.monotonic() - request_start)
            writer.close()
            try:
                await writer.wait_closed()
            except Exception as exc:
                logger.warning("[%s] Error while closing connection: %s", request_id, exc)
            logger.info("[%s] Connection closed remote=%s:%s", request_id, peer[0], peer[1])

    # ------------------------------------------------------------------
    @staticmethod
    async def _send_error(writer: asyncio.StreamWriter, request_id: str, message: str) -> None:
        payload = json.dumps({"status": "error", "message": message}) + "\n"
        logger.warning("[%s] Sending error response: %s", request_id, message)
        writer.write(payload.encode("utf-8"))
        try:
            await writer.drain()
            logger.info("[%s] Error response flushed", request_id)
        except Exception as exc:
            logger.warning("[%s] Failed flushing error response: %s", request_id, exc)

    # ------------------------------------------------------------------
    async def run(self) -> None:
        policy_error = self.control_plane_policy_error()
        if policy_error:
            logger.error("Game Server Manager refusing unsafe control-plane configuration: %s", policy_error)
            return

        self.load_levels()
        # Always prefer packaged dedicated server binaries (e.g. AuraServer.exe)
        # rather than launching via UnrealEditor + .uproject.
        self.server_exe = self.locate_server_exe()

        if self.server_exe is None:
            logger.warning(
                "No dedicated server executable resolved. Set AURA_SERVER_EXE or build AuraServer."
            )

        server = await asyncio.start_server(
            self.handle_client, self.listen_host, self.listen_port
        )
        addr = server.sockets[0].getsockname()
        logger.info(
            "Game Server Manager listening on %s:%d  (public_host=%s)",
            addr[0], addr[1], self.public_host,
        )

        try:
            async with server:
                await server.serve_forever()
        except asyncio.CancelledError:
            pass
        finally:
            logger.info("Stopping all managed dedicated servers...")
            for entry in self.levels.values():
                entry.stop()
            logger.info("Game Server Manager shut down cleanly")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Aura Game Server Manager — manages dedicated server processes "
                    "and routes clients to the correct instance via TCP."
    )
    parser.add_argument(
        "--host", default="127.0.0.1",
        help="Address to listen on (default: 127.0.0.1; use an explicit address only with LAN/public access controls)"
    )
    parser.add_argument(
        "--port", type=int, default=9000,
        help="TCP port to listen on (default: 9000)"
    )
    parser.add_argument(
        "--public-host", default="127.0.0.1",
        help="Hostname/IP clients use to reach dedicated servers (default: 127.0.0.1)"
    )
    parser.add_argument(
        "--server-exe", default=None,
        help="Path to the Aura dedicated server executable. "
             "Overrides AURA_SERVER_EXE environment variable."
    )
    parser.add_argument(
        "--persistence-provider",
        default=os.environ.get("AURA_PERSISTENCE_PROVIDER"),
        help="Development provider expected by managed servers (for example NULL or Steam). "
             "The launcher supplies NULL by default; direct manager runs keep the project setting."
    )
    parser.add_argument(
        "--startup-grace", type=float, default=12.0,
        help="Process startup grace in seconds; authenticated world readiness is still required (default: 12)"
    )
    parser.add_argument(
        "--auth-token",
        default=os.environ.get(AUTH_TOKEN_ENV, ""),
        help=f"Control-plane request token (default: {AUTH_TOKEN_ENV}; never logged)",
    )
    parser.add_argument(
        "--server-auth-token",
        default=os.environ.get(SERVER_AUTH_TOKEN_ENV, ""),
        help=f"Dedicated-server readiness token (default: {SERVER_AUTH_TOKEN_ENV} or {AUTH_TOKEN_ENV}; never logged)",
    )
    parser.add_argument(
        "--allowed-client",
        action="append",
        default=None,
        help=f"Allowed client IP/CIDR; repeatable (default: {ALLOWED_CLIENTS_ENV})",
    )
    args = parser.parse_args()

    try:
        manager = GameServerManager(
            listen_host=args.host,
            listen_port=args.port,
            public_host=args.public_host,
            server_exe_override=args.server_exe,
            startup_grace=args.startup_grace,
            persistence_provider_override=args.persistence_provider,
            auth_token=args.auth_token,
            server_auth_token=args.server_auth_token,
            allowed_clients=args.allowed_client,
        )
    except ValueError as exc:
        parser.error(str(exc))

    policy_error = manager.control_plane_policy_error()
    if policy_error:
        logger.error("Game Server Manager refusing unsafe control-plane configuration: %s", policy_error)
        raise SystemExit(2)

    try:
        asyncio.run(manager.run())
    except KeyboardInterrupt:
        logger.info("Interrupted by user")


if __name__ == "__main__":
    main()
