#!/usr/bin/env python3
"""
Simple HTTP server that serves the AuraAbilityGraph editor folder and handles
ability XML / JSON config persistence.

Usage: python ability_graph_server.py <port> <serve_dir>

Endpoints:
  GET  /list-xml           -> scan project + plugins for .xml files in Content/
  GET  /list-abilities     -> parse all ability XMLs and return name/tag list
  POST /save               -> save ability XML to disk
  POST /load               -> load XML by path
  POST /load-ability-info  -> load Content/Config/AbilityInfo.json
  POST /save-ability-info  -> save Content/Config/AbilityInfo.json
  POST /load-role-config   -> load Content/Config/RoleConfig.json
  POST /save-role-config   -> save Content/Config/RoleConfig.json
"""
from http.server import SimpleHTTPRequestHandler, HTTPServer
import sys, os, json, urllib.parse, re
from datetime import datetime


class AuraAbilityGraphHandler(SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/list-xml':
            self._handle_list_xml()
            return
        if self.path == '/list-abilities':
            self._handle_list_abilities()
            return
        return super().do_GET()

    def do_POST(self):
        if self.path == '/save':
            self._handle_save()
            return
        if self.path == '/load':
            self._handle_load()
            return
        if self.path == '/load-ability-info':
            self._handle_load_json('Content/Config/AbilityInfo.json')
            return
        if self.path == '/save-ability-info':
            self._handle_save_json('Content/Config/AbilityInfo.json')
            return
        if self.path == '/load-role-config':
            self._handle_load_json('Content/Config/RoleConfig.json')
            return
        if self.path == '/save-role-config':
            self._handle_save_json('Content/Config/RoleConfig.json')
            return
        self.send_response(404)
        self.end_headers()
        self.wfile.write(b'Not Found')

    # ── Save XML ──────────────────────────────────────────────
    def _handle_save(self):
        length = int(self.headers.get('Content-Length', '0'))
        raw = self.rfile.read(length)
        try:
            data = json.loads(raw.decode('utf-8'))
        except Exception:
            self.send_response(400)
            self.end_headers()
            self.wfile.write(b'Bad JSON')
            return

        filename = data.get('filename') or ''
        xml = data.get('xml') or ''
        source_path = data.get('source_path') or data.get('path') or ''

        if not xml:
            self.send_response(400)
            self.end_headers()
            self.wfile.write(b'Empty xml')
            return

        save_path = ''
        if source_path:
            norm_path = os.path.normpath(source_path)
            if not os.path.isabs(norm_path):
                norm_path = os.path.normpath(os.path.join(os.getcwd(), norm_path))
            if not norm_path.lower().endswith('.xml'):
                self.send_response(400)
                self.end_headers()
                self.wfile.write(b'Invalid target extension; expected .xml')
                return
            save_path = norm_path
            parent_dir = os.path.dirname(save_path)
            if parent_dir:
                os.makedirs(parent_dir, exist_ok=True)
        else:
            filename = os.path.basename(filename)
            if not filename or not re.match(r'^[\w\-. ]+$', filename):
                filename = 'Ability_%s.xml' % datetime.utcnow().strftime('%Y%m%d%H%M%S')
            save_dir = os.path.join(os.getcwd(), 'saved_abilities')
            os.makedirs(save_dir, exist_ok=True)
            save_path = os.path.join(save_dir, filename)

        try:
            with open(save_path, 'w', encoding='utf-8', newline='\n') as f:
                f.write(xml)
        except Exception:
            self.send_response(500)
            self.end_headers()
            self.wfile.write(b'Failed to write file')
            return

        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        resp = {'ok': True, 'path': save_path}
        self.wfile.write(json.dumps(resp).encode('utf-8'))

    # ── Load XML ──────────────────────────────────────────────
    def _handle_load(self):
        length = int(self.headers.get('Content-Length', '0'))
        raw = self.rfile.read(length)
        try:
            data = json.loads(raw.decode('utf-8'))
            file_path = data.get('path', '')
            if not file_path or not os.path.isfile(file_path):
                self.send_response(404)
                self.end_headers()
                return
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            resp = {'ok': True, 'path': os.path.abspath(file_path), 'content': content}
            self.wfile.write(json.dumps(resp).encode('utf-8'))
        except Exception:
            self.send_response(500)
            self.end_headers()

    # ── List XML files ────────────────────────────────────────
    def _handle_list_xml(self):
        xml_files = []
        seen_paths = set()
        project_root = os.path.abspath(os.path.join(os.getcwd(), '..', '..', '..'))
        content_roots = [os.path.join(project_root, 'Content')]
        plugins_dir = os.path.join(project_root, 'Plugins')
        if os.path.isdir(plugins_dir):
            for entry in os.listdir(plugins_dir):
                plugin_path = os.path.join(plugins_dir, entry)
                if not os.path.isdir(plugin_path):
                    continue
                direct_content = os.path.join(plugin_path, 'Content')
                if os.path.isdir(direct_content):
                    content_roots.append(direct_content)
                for sub in os.listdir(plugin_path):
                    sub_content = os.path.join(plugin_path, sub, 'Content')
                    if os.path.isdir(sub_content):
                        content_roots.append(sub_content)

        skip_dirs = {'Collections', 'Developers', 'Movies', 'Audio', 'Textures',
                     'Materials', 'Meshes', 'Models', 'Internal', 'Binaries',
                     'Intermediate', 'Generated', 'Saved', 'Config'}
        max_depth = 8

        for content_folder in content_roots:
            if not os.path.isdir(content_folder):
                continue
            for root, dirs, files in os.walk(content_folder):
                depth = root[len(content_folder):].count(os.sep)
                if depth >= max_depth:
                    dirs[:] = []
                    continue
                dirs[:] = [d for d in dirs if d not in skip_dirs and not d.startswith('.')]
                for f in files:
                    if not f.lower().endswith('.xml'):
                        continue
                    full_path = os.path.abspath(os.path.join(root, f))
                    if full_path in seen_paths:
                        continue
                    seen_paths.add(full_path)
                    rel_path = os.path.relpath(full_path, project_root)
                    xml_files.append({'path': full_path, 'name': rel_path})

        xml_files.sort(key=lambda item: item['name'].lower())
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps({'ok': True, 'files': xml_files}).encode('utf-8'))

    # ── List abilities ────────────────────────────────────────
    def _handle_list_abilities(self):
        project_root = os.path.abspath(os.path.join(os.getcwd(), '..', '..', '..'))
        content_roots = [os.path.join(project_root, 'Content')]
        plugins_dir = os.path.join(project_root, 'Plugins')
        if os.path.isdir(plugins_dir):
            for entry in os.listdir(plugins_dir):
                plugin_path = os.path.join(plugins_dir, entry)
                if not os.path.isdir(plugin_path):
                    continue
                direct_content = os.path.join(plugin_path, 'Content')
                if os.path.isdir(direct_content):
                    content_roots.append(direct_content)
                for sub in os.listdir(plugin_path):
                    sub_content = os.path.join(plugin_path, sub, 'Content')
                    if os.path.isdir(sub_content):
                        content_roots.append(sub_content)

        abilities = []
        seen = set()
        for content_folder in content_roots:
            if not os.path.isdir(content_folder):
                continue
            for root, dirs, files in os.walk(content_folder):
                dirs[:] = [d for d in dirs if d not in {'Collections','Developers','Movies','Audio','Textures','Materials','Meshes','Models','Internal','Binaries','Intermediate','Generated','Saved','Config'} and not d.startswith('.')]
                for f in files:
                    if not f.lower().endswith('.xml'):
                        continue
                    full_path = os.path.abspath(os.path.join(root, f))
                    if full_path in seen:
                        continue
                    seen.add(full_path)
                    try:
                        with open(full_path, 'r', encoding='utf-8') as fh:
                            content = fh.read()
                        name_match = re.search(r'<ability\s+[^>]*name="([^"]+)"', content)
                        tag_match = re.search(r'abilityTag="([^"]+)"', content)
                        if name_match:
                            abilities.append({
                                'name': name_match.group(1),
                                'tag': tag_match.group(1) if tag_match else '',
                                'path': full_path,
                            })
                    except Exception:
                        pass

        abilities.sort(key=lambda a: a['name'].lower())
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps({'ok': True, 'abilities': abilities}).encode('utf-8'))

    # ── JSON helpers ──────────────────────────────────────────
    def _resolve_config_path(self, rel_path):
        project_root = os.path.abspath(os.path.join(os.getcwd(), '..', '..', '..'))
        path = os.path.join(project_root, rel_path)
        return path

    def _handle_load_json(self, rel_path):
        length = int(self.headers.get('Content-Length', '0'))
        raw = self.rfile.read(length) if length else b'{}'
        try:
            data = json.loads(raw.decode('utf-8'))
        except Exception:
            data = {}
        file_path = self._resolve_config_path(rel_path)
        if not os.path.isfile(file_path):
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b'File not found')
            return
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            resp = {'ok': True, 'path': file_path, 'content': content}
            self.wfile.write(json.dumps(resp).encode('utf-8'))
        except Exception:
            self.send_response(500)
            self.end_headers()

    def _handle_save_json(self, rel_path):
        length = int(self.headers.get('Content-Length', '0'))
        raw = self.rfile.read(length)
        try:
            data = json.loads(raw.decode('utf-8'))
        except Exception:
            self.send_response(400)
            self.end_headers()
            self.wfile.write(b'Bad JSON')
            return
        content = data.get('content') or data.get('json') or ''
        file_path = self._resolve_config_path(rel_path)
        parent_dir = os.path.dirname(file_path)
        if parent_dir:
            os.makedirs(parent_dir, exist_ok=True)
        try:
            with open(file_path, 'w', encoding='utf-8', newline='\n') as f:
                f.write(content)
        except Exception:
            self.send_response(500)
            self.end_headers()
            self.wfile.write(b'Failed to write file')
            return
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        resp = {'ok': True, 'path': file_path}
        self.wfile.write(json.dumps(resp).encode('utf-8'))


def run(port, serve_dir):
    os.chdir(serve_dir)
    server = HTTPServer(('0.0.0.0', port), AuraAbilityGraphHandler)
    print('AuraAbilityGraph editor server serving %s on port %d' % (serve_dir, port))
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print('Shutting down')
        server.shutdown()


def main():
    if len(sys.argv) < 3:
        print('Usage: python ability_graph_server.py <port> <serve_dir>')
        sys.exit(2)
    try:
        port = int(sys.argv[1])
    except ValueError:
        print('Bad port')
        sys.exit(2)
    serve_dir = sys.argv[2]
    if not os.path.isdir(serve_dir):
        print('serve_dir does not exist:', serve_dir)
        sys.exit(2)
    run(port, serve_dir)


if __name__ == '__main__':
    main()
