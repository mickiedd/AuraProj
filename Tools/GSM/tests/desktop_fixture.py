"""Start a native UI fixture, close its window manually, then verify cleanup.

No window-input automation: only test process startup and TCP/HTTP/read-only
process inspection. Run start, inspect/close with the UI, then verify-closed.
"""
import argparse
import json
import os
import socket
import subprocess
import time
import urllib.request
from integration import BUILD, ROOT, process_alive

work = ROOT / 'Saved/GSMNative/webview-validation/desktop-fixture'
work.mkdir(parents=True, exist_ok=True)
state_file = work / 'processes.json'
parser = argparse.ArgumentParser()
parser.add_argument('action', choices=['start', 'verify-closed'])
action = parser.parse_args().action
if action == 'start':
    for port in (19100, 19180, 19190):
        with socket.socket() as check:
            check.bind(('127.0.0.1', port))
    config = {'levels': [{'id': 'fixture', 'displayName': 'Desktop lifecycle fixture',
                         'mapPath': '/Game/fixture', 'port': 19190,
                         'launchArgs': ['-fixture=desktop']}]}
    config_file = work / 'levels.json'
    config_file.write_text(json.dumps(config), encoding='utf-8')
    environment = dict(os.environ, AURA_GSM_AUTH_TOKEN='', AURA_GSM_SERVER_AUTH_TOKEN='',
                       AURA_GSM_HOST='127.0.0.1')
    args = [str(BUILD/'AuraGSM.exe'), '--project-root', str(work), '--config', str(config_file),
            '--server-exe', str(BUILD/'GSMFixture.exe'), '--port', '19100', '--web-port', '19180']
    with (work/'manager.log').open('w') as log:
        manager = subprocess.Popen(args, env=environment, stdout=log, stderr=log)
    try:
        deadline = time.monotonic() + 10
        while True:
            if manager.poll() is not None:
                raise RuntimeError('Desktop fixture manager exited before listening')
            try:
                with socket.create_connection(('127.0.0.1', 19100), timeout=5) as sock:
                    sock.sendall(b'{"action":"request_server","levelId":"fixture"}\n')
                    with sock.makefile('rb') as stream:
                        response = json.loads(stream.readline())
                break
            except ConnectionRefusedError:
                if time.monotonic() > deadline:
                    raise
                time.sleep(.1)
        assert response['status'] == 'ready', response
        with urllib.request.urlopen('http://127.0.0.1:19180/api/status', timeout=5) as response:
            snapshot = json.load(response)
        assert snapshot['counts']['running'] == snapshot['counts']['ready'] == 1
        state_file.write_text(json.dumps({'manager': manager.pid, 'child': snapshot['levels'][0]['pid']}))
        print('PASS: native fixture has one live, ready child. Inspect its WebView, then close the window.')
    except BaseException:
        manager.terminate(); manager.wait(5)
        raise
else:
    state = json.loads(state_file.read_text())
    deadline = time.monotonic() + 5
    while any(process_alive(pid) for pid in state.values()) and time.monotonic() < deadline:
        time.sleep(.1)
    assert not any(process_alive(pid) for pid in state.values()), 'Manager or child survived window close'
    for port in (19100, 19180):
        with socket.socket() as sock:
            sock.settimeout(1)
            assert sock.connect_ex(('127.0.0.1', port)) != 0, 'Listener survived window close'
    print('PASS: window close stopped GSM, killed its owned DS fixture, and released both listeners.')
