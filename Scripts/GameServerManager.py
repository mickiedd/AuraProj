#!/usr/bin/env python3
"""
Aura Game Server Manager

Manages dedicated Unreal Engine server processes and routes clients
to the correct instance via TCP.

Protocol (newline-delimited JSON over TCP):
  Client -> Server: {"action": "request_server", "levelId": "<id>"}
  Server -> Client: {"status": "ready", "host": "<host>", "port": <port>}
               or: {"status": "error", "message": "<reason>"}

Usage:
  python GameServerManager.py [--host 0.0.0.0] [--port 9000]
                               [--public-host 127.0.0.1]
                               [--server-exe /path/to/AuraServer]
                               [--startup-grace 12]
"""

import argparse
import asyncio
import json
import logging
import os
import subprocess
import sys
import time
from pathlib import Path
from typing import Dict, List, Optional

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
LEVEL_CONFIG_PATH = PROJECT_DIR / "Config" / "LevelConfig.json"
GSM_LOG_DIR = PROJECT_DIR / "Saved" / "Logs" / "GameServerManager"

# Maximum bytes accepted in a single client request line (prevents abuse).
MAX_REQUEST_BYTES = 4096

# ---------------------------------------------------------------------------
# Level / server entry
# ---------------------------------------------------------------------------

class DedicatedServerEntry:
    """Tracks a single dedicated server process for one level."""

    def __init__(self, level_cfg: dict, startup_grace: float):
        self.level_id: str = level_cfg["id"]
        self.display_name: str = level_cfg.get("displayName", self.level_id)
        self.map_path: str = level_cfg["mapPath"]
        self.port: int = int(level_cfg["port"])
        self.query_port: int = int(level_cfg.get("queryPort", 0))
        self.launch_args: List[str] = level_cfg.get(
            "launchArgs", ["-game", "-server", "-log", "-unattended", "-NoLiveCoding"]
        )
        self.startup_grace: float = startup_grace

        self._process: Optional[subprocess.Popen] = None
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

        if server_exe.name.lower() == "unrealeditor.exe":
            args = [str(server_exe), str(PROJECT_FILE), map_arg] + list(self.launch_args)
            launch_mode = "editor"
        else:
            args = [str(server_exe), map_arg] + list(self.launch_args)
            launch_mode = "server"

        args.append(f"-port={self.port}")
        if self.query_port > 0:
            args.append(f"-QueryPort={self.query_port}")
        args.append(f"-Abslog={log_file}")

        logger.info("Starting DS '%s' via %s launcher: %s", self.level_id, launch_mode, " ".join(args))
        logger.info("DS '%s' log file: %s", self.level_id, log_file)
        try:
            self._process = subprocess.Popen(
                args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
            )
        except Exception as exc:
            logger.error("Failed to launch DS '%s': %s", self.level_id, exc)
            self._process = None
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
            "DS '%s' is ready on port %d (pid=%d, elapsed=%.3fs)",
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
    ):
        self.listen_host = listen_host
        self.listen_port = listen_port
        self.public_host = public_host
        self.server_exe_override = server_exe_override
        self.startup_grace = startup_grace

        self.levels: Dict[str, DedicatedServerEntry] = {}
        self.server_exe: Optional[Path] = None
        self.request_counter = 0
        self.ready_servers: Dict[str, dict] = {}
        self.ready_events: Dict[str, asyncio.Event] = {}

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

            entry = DedicatedServerEntry(entry_cfg, self.startup_grace)
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

        candidates = [
            PROJECT_DIR / "Binaries" / "Win64" / "AuraServer.exe",
            PROJECT_DIR / "Binaries" / "Mac" / "AuraServer",
            PROJECT_DIR / "Binaries" / "Mac" / "Aura-Mac-Shipping",
        ]
        for candidate in candidates:
            if candidate.exists():
                logger.info("Found server executable: %s", candidate)
                return candidate

        logger.warning(
            "No server executable found. Dedicated servers must be started manually, "
            "or set AURA_SERVER_EXE env var / --server-exe flag."
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

            action = request.get("action", "")
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

                announced_host = str(request.get("host", "")).strip() or str(peer[0])
                self.ready_servers[level_id] = {
                    "host": announced_host,
                    "port": ready_port,
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
                "[%s] Request accepted remote=%s:%s levelId='%s'",
                request_id,
                peer[0],
                peer[1],
                level_id,
            )

            was_running_before = entry.is_running()
            if not was_running_before:
                self._clear_ready_state(level_id)
                logger.info("[%s] Cleared stale ready state for levelId='%s' before launch", request_id, level_id)

            ok = await entry.ensure_running(self.server_exe)
            if not ok:
                logger.error("[%s] Failed ensuring DS is running for levelId='%s'", request_id, level_id)
                await self._send_error(
                    writer,
                    request_id,
                    f"Dedicated server for '{level_id}' could not be started. "
                    "Please start it manually and retry.",
                )
                return

            ready_info = await self._wait_for_ready(level_id, entry.port, self.startup_grace + 20.0)
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
                    f"Dedicated server for '{level_id}' did not report ready in time.",
                )
                return

            response = {
                "status": "ready",
                "host": str(ready_info.get("host", self.public_host)),
                "port": int(ready_info.get("port", entry.port)),
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
        self.load_levels()
        if os.name == "nt":
            self.server_exe = self.locate_editor_exe() or self.locate_server_exe()
        else:
            self.server_exe = self.locate_editor_exe() or self.locate_server_exe()

        if self.server_exe is None:
            logger.warning(
                "No launcher executable resolved. Set AURA_SERVER_EXE or UE_EDITOR_EXE to enable DS startup."
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
        "--host", default="0.0.0.0",
        help="Address to listen on (default: 0.0.0.0)"
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
        "--startup-grace", type=float, default=12.0,
        help="Seconds to wait after launching a DS before declaring it ready (default: 12)"
    )
    args = parser.parse_args()

    manager = GameServerManager(
        listen_host=args.host,
        listen_port=args.port,
        public_host=args.public_host,
        server_exe_override=args.server_exe,
        startup_grace=args.startup_grace,
    )

    try:
        asyncio.run(manager.run())
    except KeyboardInterrupt:
        logger.info("Interrupted by user")


if __name__ == "__main__":
    main()
