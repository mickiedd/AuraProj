# remote_run.py  — bash-side client for UE Python Remote Execution.
# Usage (from a normal shell, NOT the editor):
#   python Scripts/remote_run.py Scripts/_diag_trace.py
#   python Scripts/remote_run.py Scripts/populate_desert_houses.py
#
# Requires: Project Settings > Plugins > Python > "Enable Remote Execution?" = ON
# in the running editor. Discovers the editor's remote node, opens a command
# connection, executes the given script file remotely, and prints the result.

import os
import sys
import time
import logging

# Engine's remote_execution.py (the client implementation of the protocol).
ENGINE_REMOTE = r"C:/Git/UnrealEngine-5.5/Engine/Plugins/Experimental/PythonScriptPlugin/Content/Python"
sys.path.insert(0, ENGINE_REMOTE)

import remote_execution as _re  # noqa: E402

_re.set_log_level(logging.WARNING)


def main():
    if len(sys.argv) < 2:
        print("usage: python remote_run.py <script.py>")
        return 2
    script_path = os.path.abspath(sys.argv[1])
    # ExecuteFile runs a file by path cleanly (sending inline multi-line code
    # is unreliable — the editor tries to resolve it as a file path). Forward
    # slashes avoid backslash-escaping issues in the JSON command payload.
    code = script_path.replace("\\", "/")

    rx = _re.RemoteExecution()
    rx.start()
    try:
        # discover the editor node
        node = None
        deadline = time.time() + 15.0
        while time.time() < deadline:
            nodes = rx.remote_nodes
            if nodes:
                node = nodes[0]
                break
            time.sleep(0.4)
        if node is None:
            print("ERROR: no remote editor node discovered. "
                  "Is 'Enable Remote Execution?' ON in Project Settings > Python?")
            return 1
        print("discovered editor node: {}".format(node.get("node_id")))
        rx.open_command_connection(node["node_id"])
        print("connected; running {} ({} bytes)...".format(script_path, len(code)))
        res = rx.run_command(code, unattended=True, exec_mode=_re.MODE_EXEC_FILE,
                             raise_on_failure=False)
        print("---- command_result ----")
        print("success: {}".format(res.get("success")))
        out = res.get("output")
        if out:
            print("---- output ----")
            print(out)
        result = res.get("result")
        if result:
            print("---- result ----")
            print(result)
        rx.close_command_connection()
    finally:
        rx.stop()
    return 0


if __name__ == "__main__":
    sys.exit(main())