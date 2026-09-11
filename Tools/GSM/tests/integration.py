"""Native GSM black-box integration tests. Python is a test dependency only."""
import concurrent.futures
import ctypes
import http.client
import json
import os
from pathlib import Path
import socket
import shutil
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[3]
BUILD = ROOT / 'Saved/GSMNative/build/Release'
EXE = BUILD / 'AuraGSM.exe'
FIXTURE = BUILD / 'GSMFixture.exe'


def free_port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def process_alive(pid):
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.OpenProcess.restype = ctypes.c_void_p
    kernel.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    handle = kernel.OpenProcess(0x100000, False, pid)
    if not handle:
        return False
    alive = kernel.WaitForSingleObject(handle, 0) == 258
    kernel.CloseHandle(handle)
    return alive


def terminate_pid(pid):
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.OpenProcess.restype = ctypes.c_void_p
    kernel.TerminateProcess.argtypes = [ctypes.c_void_p, ctypes.c_uint]
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    handle = kernel.OpenProcess(0x0001, False, pid)
    if not handle:
        raise OSError(ctypes.get_last_error(), f'OpenProcess({pid}) failed')
    try:
        if not kernel.TerminateProcess(handle, 99):
            raise OSError(ctypes.get_last_error(), f'TerminateProcess({pid}) failed')
    finally:
        kernel.CloseHandle(handle)


class NativeGSM(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='gsm native ')
        self.dir = Path(self.temp.name)
        self.tcp, self.web, self.game = free_port(), free_port(), free_port()
        while len({self.tcp, self.web, self.game}) < 3:
            self.tcp, self.web, self.game = free_port(), free_port(), free_port()
        self.process = None
        self.config = self.dir / 'levels.json'
        self.record = self.dir / 'child.json'
        self.env = dict(os.environ, AURA_GSM_AUTH_TOKEN='client-private-token',
                        AURA_GSM_SERVER_AUTH_TOKEN='server-private-token',
                        AURA_GSM_HOST='127.0.0.1', GSM_FIXTURE_RECORD=str(self.record))
        self.write_config()

    def write_config(self, mode='ready', **extra):
        self.data = {'gameServerPort': self.tcp, 'levels': [dict(
            id='fixture', displayName='<img src=x onerror=alert(1)>', mapPath='/Game/fixture',
            port=self.game, launchArgs=[f'-fixture={mode}', '-Password=do-not-display',
                                       '-Token', 'also-private', '-quoted=a "b" c\\'], **extra)]}
        self.config.write_text(json.dumps(self.data), encoding='utf-8')

    def args(self):
        return [str(EXE), '--headless', '--project-root', str(self.dir), '--config', str(self.config),
                '--port', str(self.tcp), '--web-port', str(self.web),
                '--server-exe', str(FIXTURE), '--public-host', 'configured.example']

    def start(self):
        self.process = subprocess.Popen(self.args(), env=self.env, stdout=subprocess.DEVNULL,
                                        stderr=subprocess.DEVNULL, creationflags=subprocess.CREATE_NO_WINDOW)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                self.fail('Manager failed startup')
            try:
                self.status()
                return
            except OSError:
                time.sleep(.05)
        self.fail('Manager startup timed out')

    def tearDown(self):
        if self.process and self.process.poll() is None:
            self.process.terminate()
            self.process.wait(5)
        self.temp.cleanup()

    def http(self, path='/api/status', host=None, method='GET'):
        connection = http.client.HTTPConnection('127.0.0.1', self.web, timeout=3)
        connection.request(method, path, headers={'Host': host or f'127.0.0.1:{self.web}'})
        response = connection.getresponse()
        result = response.status, response.read(), dict(response.getheaders())
        connection.close()
        return result

    def status(self):
        code, body, _ = self.http()
        self.assertEqual(code, 200)
        return json.loads(body)

    def request(self, **fields):
        request = dict(action='request_server', levelId='fixture', authToken='client-private-token')
        request.update(fields)
        return self.raw(json.dumps(request).encode() + b'\n')

    def raw(self, payload):
        with socket.create_connection(('127.0.0.1', self.tcp), timeout=35) as sock:
            sock.sendall(payload)
            with sock.makefile('rb') as stream:
                return json.loads(stream.readline())

    def wait_for(self, predicate, timeout=5):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            value = predicate()
            if value:
                return value
            time.sleep(.05)
        self.fail('Condition timed out')

    def test_concurrent_readiness_metrics_redaction_and_shutdown(self):
        self.start()
        self.assertEqual(self.status()['counts']['running'], 0)
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            futures = [pool.submit(self.request) for _ in range(8)]
            self.wait_for(lambda: self.status()['counts']['starting'] == 1)
            self.assertEqual(self.status()['counts']['ready'], 0)
            results = [future.result() for future in futures]
        self.assertTrue(all(row['status'] == 'ready' for row in results))
        self.assertTrue(all(row['host'] == 'configured.example' for row in results))
        snapshot = self.status()
        row = snapshot['levels'][0]
        self.assertEqual(snapshot['counts']['running'], 1)
        self.assertEqual(row['launchCount'], 1)
        self.assertGreater(row['workingSetBytes'], 0)
        self.assertGreaterEqual(row['cpuSeconds'], 0)
        child = json.loads(self.record.read_text())
        self.assertIn('-quoted=a "b" c\\', child['args'])
        self.assertIn(str(self.dir), row['logPath'])
        for secret in ['do-not-display', 'also-private', 'client-private-token', 'server-private-token', child['nonce']]:
            self.assertNotIn(secret, json.dumps(snapshot))
        self.process.terminate()
        self.process.wait(5)
        self.wait_for(lambda: not process_alive(row['pid']))

    def test_auth_invalid_input_http_and_nonce_guards(self):
        self.start()
        for fields in [{'authToken': 'wrong'}, {'levelId': 'unknown'}, {'action': 'delete_all'}, {'levelId': 7}]:
            self.assertEqual(self.request(**fields)['status'], 'error')
        for payload in [b'[]\n', b'{bad json}\n', b'x' * 4097 + b'\n']:
            self.assertEqual(self.raw(payload)['status'], 'error')
        self.assertEqual(self.request(action='server_ready', serverAuthToken='server-private-token',
                                      port=self.game, readyNonce='invented')['status'], 'error')
        self.assertEqual(self.status()['counts']['running'], 0)
        self.assertEqual(self.http(host='evil.example')[0], 403)
        self.assertEqual(self.http('/../levels.json')[0], 404)
        self.assertEqual(self.http(method='POST')[0], 405)
        self.assertIn('frame-ancestors', self.http('/')[2]['Content-Security-Policy'])
        self.assertEqual(self.http('/app.js')[0], 200)

    def test_live_nonce_port_auth_and_endpoint_authority(self):
        self.write_config('timeout')
        self.start()
        with concurrent.futures.ThreadPoolExecutor() as pool:
            future = pool.submit(self.request)
            self.wait_for(self.record.exists)
            child = json.loads(self.record.read_text())
            base = dict(action='server_ready', serverAuthToken='server-private-token',
                        port=self.game, readyNonce=child['nonce'])
            for replacement in [{'port':self.game+1}, {'port':self.game + .5}, {'port':self.game + 2**32}, {'readyNonce':'wrong'}, {'serverAuthToken':'wrong'}]:
                self.assertEqual(self.request(**dict(base, **replacement))['status'], 'error')
            self.assertEqual(self.status()['counts']['ready'], 0)
            self.assertEqual(self.request(**dict(base, host='attacker.example'))['status'], 'ok')
            self.assertEqual(future.result()['host'], 'configured.example')

    def test_request_deadline_preserves_startup_and_accepts_late_readiness(self):
        self.write_config('timeout')
        self.data['requestDeadlineSeconds'] = .5
        self.data['serverStartupDeadlineSeconds'] = 3
        self.config.write_text(json.dumps(self.data), encoding='utf-8')
        self.start()
        started = time.monotonic()
        result = self.request()
        elapsed = time.monotonic() - started
        self.assertEqual(result['status'], 'error')
        self.assertTrue(result['retryable'])
        self.assertIn('startup continues in the background', result['message'])
        self.assertGreaterEqual(elapsed, .4)
        self.assertLess(elapsed, 2)
        row = self.status()['levels'][0]
        child = json.loads(self.record.read_text())
        self.assertEqual(row['state'], 'starting')
        self.assertTrue(row['running'])
        self.assertTrue(process_alive(row['pid']))
        self.assertEqual(row['launchCount'], 1)
        self.assertEqual(self.request(action='server_ready', serverAuthToken='server-private-token',
                                      port=self.game, readyNonce=child['nonce'])['status'], 'ok')
        self.assertEqual(self.request()['status'], 'ready')
        after = self.status()['levels'][0]
        self.assertEqual(after['pid'], row['pid'])
        self.assertEqual(after['launchCount'], 1)

    def test_stale_readiness_from_replaced_launch_is_rejected(self):
        self.write_config('timeout')
        self.data['requestDeadlineSeconds'] = .2
        self.data['serverStartupDeadlineSeconds'] = 3
        self.config.write_text(json.dumps(self.data), encoding='utf-8')
        self.start()

        self.assertTrue(self.request()['retryable'])
        first = self.status()['levels'][0]
        first_child = json.loads(self.record.read_text())
        first_nonce = first_child['nonce']
        terminate_pid(first['pid'])
        self.wait_for(lambda: self.status()['levels'][0]['state'] == 'exited')

        self.assertTrue(self.request()['retryable'])
        second = self.status()['levels'][0]
        second_child = json.loads(self.record.read_text())
        self.assertNotEqual(second['pid'], first['pid'])
        self.assertNotEqual(second_child['nonce'], first_nonce)
        stale = self.request(action='server_ready', serverAuthToken='server-private-token',
                              port=self.game, readyNonce=first_nonce)
        self.assertEqual(stale['status'], 'error')
        self.assertEqual(self.status()['levels'][0]['state'], 'starting')
        self.assertEqual(self.request(action='server_ready', serverAuthToken='server-private-token',
                                      port=self.game, readyNonce=second_child['nonce'])['status'], 'ok')
        self.assertEqual(self.request()['status'], 'ready')

    def test_startup_deadline_wins_over_late_readiness(self):
        self.write_config('timeout')
        self.data['requestDeadlineSeconds'] = .2
        self.data['serverStartupDeadlineSeconds'] = .8
        self.config.write_text(json.dumps(self.data), encoding='utf-8')
        self.start()
        self.assertTrue(self.request()['retryable'])
        pid = self.status()['levels'][0]['pid']
        child = json.loads(self.record.read_text())
        failed = self.wait_for(lambda: (
            row if (row := self.status()['levels'][0])['state'] == 'failed' else None), timeout=2)
        self.assertIn('server startup deadline', failed['error'])
        self.wait_for(lambda: not process_alive(pid))
        late = self.request(action='server_ready', serverAuthToken='server-private-token',
                            port=self.game, readyNonce=child['nonce'])
        self.assertEqual(late['status'], 'error')
        self.assertEqual(self.status()['levels'][0]['state'], 'failed')

    def test_request_disconnect_does_not_cleanup_starting_child(self):
        self.write_config('timeout')
        self.data['requestDeadlineSeconds'] = .5
        self.data['serverStartupDeadlineSeconds'] = 3
        self.config.write_text(json.dumps(self.data), encoding='utf-8')
        self.start()
        payload = json.dumps(dict(action='request_server', levelId='fixture',
                                  authToken='client-private-token')).encode() + b'\n'
        with socket.create_connection(('127.0.0.1', self.tcp), timeout=3) as sock:
            sock.sendall(payload)
        row = self.wait_for(lambda: (
            value if (value := self.status()['levels'][0])['state'] == 'starting' else None))
        self.assertTrue(process_alive(row['pid']))
        self.wait_for(self.record.exists)
        child = json.loads(self.record.read_text())
        self.assertEqual(self.request(action='server_ready', serverAuthToken='server-private-token',
                                      port=self.game, readyNonce=child['nonce'])['status'], 'ok')
        self.assertEqual(self.request()['status'], 'ready')

    def test_server_startup_deadline_eventually_cleans_up(self):
        self.write_config('timeout')
        self.data['requestDeadlineSeconds'] = .2
        self.data['serverStartupDeadlineSeconds'] = .8
        self.config.write_text(json.dumps(self.data), encoding='utf-8')
        self.start()
        result = self.request()
        self.assertEqual(result['status'], 'error')
        self.assertTrue(result['retryable'])
        self.assertIn('startup continues in the background', result['message'])
        pid = self.status()['levels'][0]['pid']
        row = self.wait_for(lambda: (
            value if (value := self.status()['levels'][0])['state'] == 'failed' else None), timeout=2)
        self.assertIn('server startup deadline', row['error'])
        self.wait_for(lambda: not process_alive(pid))

    def test_early_exit_and_failed_launch(self):
        self.write_config('exit')
        self.start()
        self.assertEqual(self.request()['status'], 'error')
        row = self.status()['levels'][0]
        self.assertEqual(row['exitCode'], 23)
        self.assertEqual(row['state'], 'exited')
        self.assertEqual(self.status()['counts']['running'], 0)

    def test_startup_errors_and_runtime_authority(self):
        self.start()
        duplicate = subprocess.run(self.args(), env=self.env, capture_output=True, timeout=5)
        self.assertNotEqual(duplicate.returncode, 0)
        self.assertIn(b'Cannot listen', duplicate.stderr)
        self.assertEqual(self.request(clientExecutable='UnrealEditor.exe',clientEngineRoot='C:/untrusted')['status'], 'error')
        self.assertEqual(self.status()['counts']['running'], 0)
        self.process.terminate(); self.process.wait(5)
        self.data['levels'].append(self.data['levels'][0])
        self.config.write_text(json.dumps(self.data))
        invalid = subprocess.run(self.args(), env=self.env, capture_output=True, timeout=5)
        self.assertNotEqual(invalid.returncode, 0)
        self.assertIn(b'duplicate', invalid.stderr.lower())

    def test_missing_executable_is_explicit_and_recoverable(self):
        missing = self.dir / 'later-built.exe'
        self.env['AURA_SERVER_EXE'] = str(missing)
        args = self.args()
        args[args.index('--server-exe') + 1] = str(missing)
        original_args = self.args
        self.args = lambda: args
        self.start()
        response = self.request()
        self.assertEqual(response['status'], 'error')
        self.assertIn('unavailable', response['message'])
        self.assertEqual(self.status()['counts']['running'], 0)
        shutil.copyfile(FIXTURE, missing)
        self.assertEqual(self.request()['status'], 'ready')
        self.args = original_args

    def check_automatic_editor(self, stale=False):
        project = self.dir / 'Project'
        project.mkdir()
        (project / 'Aura.uproject').write_text(json.dumps({'EngineAssociation': 'gsm-fixture'}))
        engine = self.dir / 'UE_gsm-fixture' / 'Engine'
        binaries = engine / 'Binaries' / 'Win64'
        binaries.mkdir(parents=True)
        name = 'UnrealEditor-Win64-DebugGame.exe'
        shutil.copyfile(FIXTURE, binaries / name)
        self.env.pop('UE_EDITOR_EXE', None)
        if stale:
            self.env['UE_EDITOR_EXE'] = str(self.dir / 'missing' / 'UnrealEditor.exe')
        args = self.args()
        args[args.index('--project-root') + 1] = str(project) + '/.'
        self.args = lambda: args
        self.start()
        rejected = self.request(clientExecutable=name, clientEngineRoot=str(self.dir / 'untrusted'))
        self.assertEqual(rejected['status'], 'error')
        self.assertEqual(self.status()['counts']['running'], 0)
        result = self.request(clientExecutable=name, clientEngineRoot=engine.as_posix() + '/')
        self.assertEqual(result['status'], 'ready', result)
        self.assertEqual(result['serverMode'], 'editor')
        self.assertEqual(Path(self.status()['levels'][0]['executable']), binaries / name)

    def test_automatic_editor_without_environment(self):
        self.check_automatic_editor()

    def test_automatic_editor_ignores_stale_environment(self):
        self.check_automatic_editor(stale=True)

    def test_runtime_switch_does_not_stop_live_server(self):
        configured_editor = self.dir / 'UnrealEditor.exe'
        shutil.copyfile(FIXTURE, configured_editor)
        self.env['UE_EDITOR_EXE'] = str(configured_editor)
        self.start()
        self.assertEqual(self.request()['status'], 'ready')
        before = self.status()['levels'][0]
        result = self.request(clientExecutable='UnrealEditor.exe')
        self.assertEqual(result['status'], 'error')
        self.assertIn('different runtime', result['message'])
        after = self.status()['levels'][0]
        self.assertEqual(before['pid'], after['pid'])
        self.assertEqual(after['state'], 'ready')

    def test_invalid_ports_remote_bind_and_oversize_http(self):
        self.data['levels'][0]['port'] = 7777.5
        self.config.write_text(json.dumps(self.data))
        result = subprocess.run(self.args(), env=self.env, capture_output=True, timeout=5)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b'integers', result.stderr)
        self.write_config()
        self.data['requestDeadlineSeconds'] = 'fast'
        self.config.write_text(json.dumps(self.data), encoding='utf-8')
        result = subprocess.run(self.args(), env=self.env, capture_output=True, timeout=5)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b'requestDeadlineSeconds must be a number', result.stderr)
        self.write_config()
        self.data['requestDeadlineSeconds'] = 2
        self.data['serverStartupDeadlineSeconds'] = 1
        self.config.write_text(json.dumps(self.data), encoding='utf-8')
        result = subprocess.run(self.args(), env=self.env, capture_output=True, timeout=5)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b'must be greater', result.stderr)
        for literal in ['NaN', 'Infinity', '-Infinity', '1e309']:
            self.write_config()
            self.data['requestDeadlineSeconds'] = 1.0
            raw = json.dumps(self.data).replace('"requestDeadlineSeconds": 1.0',
                                                 f'"requestDeadlineSeconds": {literal}')
            self.config.write_text(raw, encoding='utf-8')
            result = subprocess.run(self.args(), env=self.env, capture_output=True, timeout=5)
            self.assertNotEqual(result.returncode, 0, literal)
            if literal == '1e309':
                self.assertTrue(
                    b'between 0.1 and 3600' in result.stderr or b'number overflow' in result.stderr,
                    result.stderr)
        self.write_config()
        result = subprocess.run(self.args() + ['--host', '0.0.0.0'], env=self.env, capture_output=True, timeout=5)
        self.assertNotEqual(result.returncode, 0)
        self.start()
        with socket.create_connection(('127.0.0.1', self.web), timeout=3) as sock:
            sock.sendall(b'GET / HTTP/1.1\r\nHost: ' + f'127.0.0.1:{self.web}'.encode() + b'\r\nX-Large: ' + b'x' * 8300 + b'\r\n\r\n')
            self.assertIn(b'413', sock.recv(100))


if __name__ == '__main__':
    unittest.main(verbosity=2)
