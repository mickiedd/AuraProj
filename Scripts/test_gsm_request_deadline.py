"""Exercise the real TCP handler with slow/healthy dedicated-server fixtures."""
import asyncio
import json
import unittest
from pathlib import Path
from unittest.mock import patch

import GameServerManager as gsm


class RequestDeadlineTests(unittest.IsolatedAsyncioTestCase):
    async def request(self, startup_delay, ready_delay=None):
        manager = gsm.GameServerManager('127.0.0.1', 0, '127.0.0.1', None, 12)
        entry = gsm.DedicatedServerEntry(
            {'id': 'fixture', 'mapPath': '/Game/Maps/StartupMap', 'port': 17790}, 12)
        manager.levels['fixture'] = entry
        manager.server_exe = Path(__file__)
        entry.is_running = lambda: True

        async def start(_):
            await asyncio.sleep(startup_delay)
            return True
        entry.ensure_running = start

        async def publish_ready():
            await asyncio.sleep(ready_delay)
            manager.ready_servers['fixture'] = {'host': '127.0.0.1', 'port': 17790}
            manager._get_ready_event('fixture').set()

        server = await asyncio.start_server(manager.handle_client, '127.0.0.1', 0)
        task = asyncio.create_task(publish_ready()) if ready_delay is not None else None
        try:
            with patch.object(gsm, 'SERVER_REQUEST_BUDGET_SECONDS', 0.20):
                reader, writer = await asyncio.open_connection('127.0.0.1', server.sockets[0].getsockname()[1])
                writer.write(b'{"action":"request_server","levelId":"fixture"}\n')
                await writer.drain()
                response = json.loads(await asyncio.wait_for(reader.readline(), 0.6))
                self.assertEqual(await asyncio.wait_for(reader.read(), 0.6), b'')
                writer.close()
                await writer.wait_closed()
                return response
        finally:
            server.close()
            await server.wait_closed()
            if task:
                await task

    async def test_missing_readiness_returns_explicit_error(self):
        response = await self.request(0)
        self.assertEqual(response['status'], 'error')
        self.assertIn('healthy world readiness', response['message'])

    async def test_startup_wait_is_bounded(self):
        response = await self.request(2)
        self.assertEqual(response['status'], 'error')
        self.assertIn('startup request deadline', response['message'])

    async def test_launch_and_readiness_share_budget(self):
        response = await self.request(0.15, 0.30)
        self.assertEqual(response['status'], 'error')

    async def test_ready_server_returns_endpoint(self):
        response = await self.request(0.02, 0.05)
        self.assertEqual(response['status'], 'ready')
        self.assertEqual(response['port'], 17790)


if __name__ == '__main__':
    unittest.main()
