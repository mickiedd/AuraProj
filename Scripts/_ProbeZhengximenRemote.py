"""Transient diagnostic: longer, verbose UE remote-execution discovery.

remote_run.py gives discovery 15 s and stays quiet. The editor here holds
UDP 127.0.0.1:6766 but never answers, which is the documented dead-end — this
script only exists to confirm that before we escalate to closing the editor.
"""
import logging
import os
import sys
import time

ENGINE_REMOTE = os.path.join(
    os.environ.get("UE_ENGINE_ROOT", "/Volumes/M2/Engine/UE_5.5"),
    "Engine", "Plugins", "Experimental", "PythonScriptPlugin", "Content", "Python")
sys.path.insert(0, ENGINE_REMOTE)

import remote_execution as _re  # noqa: E402

_re.set_log_level(logging.DEBUG)

rx = _re.RemoteExecution()
rx.start()
print("local node:", getattr(rx, "local_node", None))
print("remote_nodes (initial):", rx.remote_nodes)

deadline = time.time() + 45.0
seen = None
while time.time() < deadline:
    nodes = rx.remote_nodes
    if nodes:
        seen = nodes
        print("discovered after {:.1f}s: {}".format(45.0 - (deadline - time.time()), nodes))
        break
    time.sleep(0.5)

if not seen:
    print("NO_NODES after 45s")
    print("final remote_nodes:", rx.remote_nodes)
else:
    node = seen[0]
    rx.open_command_connection(node["node_id"])
    res = rx.run_command("import unreal; print(unreal.SystemLibrary.get_engine_version())",
                         unattended=True, exec_mode=_re.MODE_EXEC_STATEMENT,
                         raise_on_failure=False)
    print("result:", res)

rx.stop()
