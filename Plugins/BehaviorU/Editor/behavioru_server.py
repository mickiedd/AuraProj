#!/usr/bin/env python3
"""
Simple HTTP server that serves the editor folder and accepts POST /save
to persist Behavior Tree XML files to disk.

Usage: python behavioru_server.py <port> <serve_dir>

POST /save
    Content-Type: application/json
    Body: {
        "filename": "MyTree.xml",
        "xml": "...xml content...",
        "source_path": "E:/Project/BehaviorTrees/MyTree.xml" // optional
    }

Responds with JSON: { "ok": true, "path": "<saved path>" }
"""
from http.server import SimpleHTTPRequestHandler, HTTPServer
import sys, os, json, urllib.parse, re
from datetime import datetime


class BehaviorUHandler(SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/list-xml':
            self._handle_list_xml()
            return
        return super().do_GET()

    def do_POST(self):
        if self.path == '/upload':
            self._handle_upload()
            return
        if self.path == '/load':
            self._handle_load()
            return
        if self.path != '/save':
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b'Not Found')
            return

        length = int(self.headers.get('Content-Length', '0'))
        raw = self.rfile.read(length)
        try:
            data = json.loads(raw.decode('utf-8'))
        except Exception as e:
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
            # Save back to exact original path when provided by runtime sync.
            # Relative paths are resolved against current working directory.
            norm_path = os.path.normpath(source_path)
            if not os.path.isabs(norm_path):
                norm_path = os.path.normpath(os.path.join(os.getcwd(), norm_path))

            # Restrict to XML targets for safety.
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
            # Fallback: save to local saved_trees folder using filename.
            filename = os.path.basename(filename)
            if not filename or not re.match(r'^[\w\-. ]+$', filename):
                filename = 'BehaviorTree_%s.xml' % datetime.utcnow().strftime('%Y%m%d%H%M%S')

            save_dir = os.path.join(os.getcwd(), 'saved_trees')
            os.makedirs(save_dir, exist_ok=True)
            save_path = os.path.join(save_dir, filename)

        try:
            with open(save_path, 'w', encoding='utf-8', newline='\n') as f:
                f.write(xml)
        except Exception as e:
            self.send_response(500)
            self.end_headers()
            self.wfile.write(b'Failed to write file')
            return

        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        resp = { 'ok': True, 'path': save_path }
        self.wfile.write(json.dumps(resp).encode('utf-8'))

    def _handle_upload(self):
        """处理文件上传，返回文件内容和完整路径"""
        content_type = self.headers.get('Content-Type', '')
        if 'multipart/form-data' not in content_type:
            self.send_response(400)
            self.end_headers()
            return

        # 解析 multipart/form-data
        import cgi
        form = cgi.FieldStorage(
            fp=self.rfile,
            headers=self.headers,
            environ={'REQUEST_METHOD': 'POST'}
        )

        if 'file' not in form:
            self.send_response(400)
            self.end_headers()
            return

        file_item = form['file']
        if not file_item.file:
            self.send_response(400)
            self.end_headers()
            return

        # 读取文件内容
        content = file_item.file.read().decode('utf-8')
        filename = file_item.filename

        # 保存到临时位置并返回绝对路径
        temp_dir = os.path.join(os.getcwd(), 'uploaded_trees')
        os.makedirs(temp_dir, exist_ok=True)
        temp_path = os.path.join(temp_dir, filename)

        with open(temp_path, 'w', encoding='utf-8') as f:
            f.write(content)

        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        resp = {'ok': True, 'path': os.path.abspath(temp_path), 'content': content}
        self.wfile.write(json.dumps(resp).encode('utf-8'))

    def _handle_list_xml(self):
        """List every Behavior Tree XML reachable from the project — the project's own
        Content/ folder PLUS every installed plugin's Content/ folder (e.g. BTs shipped
        inside Plugins/<Plugin>/Content/AutoTests/). Stays fast via skip-dir pruning and
        a depth cap; only Content trees are scanned (Source/Intermediate/Binaries ignored).
        """
        xml_files = []
        seen_paths = set()

        # The server runs from Plugins/BehaviorU/Editor, so ../../.. is the project root.
        project_root = os.path.abspath(os.path.join(os.getcwd(), '..', '..', '..'))

        # Collect every Content root to scan: project Content + each plugin's Content.
        content_roots = [os.path.join(project_root, 'Content')]

        plugins_dir = os.path.join(project_root, 'Plugins')
        if os.path.isdir(plugins_dir):
            for entry in os.listdir(plugins_dir):
                plugin_path = os.path.join(plugins_dir, entry)
                if not os.path.isdir(plugin_path):
                    continue
                # Plugins may live directly under Plugins/<Name>/ or nested under
                # Plugins/<Group>/<Name>/ (e.g. Plugins/Developer/RiderLink). Pick up
                # Content folders at either level.
                direct_content = os.path.join(plugin_path, 'Content')
                if os.path.isdir(direct_content):
                    content_roots.append(direct_content)
                for sub in os.listdir(plugin_path):
                    sub_content = os.path.join(plugin_path, sub, 'Content')
                    if os.path.isdir(sub_content):
                        content_roots.append(sub_content)

        # Directories that are large and never contain behavior trees — pruned in-place.
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

        # Stable, readable ordering by relative path.
        xml_files.sort(key=lambda item: item['name'].lower())

        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps({'ok': True, 'files': xml_files}).encode('utf-8'))

    def _handle_load(self):
        """加载指定路径的 XML 文件"""
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
        except:
            self.send_response(500)
            self.end_headers()


def run(port, serve_dir):
    os.chdir(serve_dir)
    server = HTTPServer(('0.0.0.0', port), BehaviorUHandler)
    print('BehaviorU editor server serving %s on port %d' % (serve_dir, port))
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print('Shutting down')
        server.shutdown()


def main():
    if len(sys.argv) < 3:
        print('Usage: python behavioru_server.py <port> <serve_dir>')
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
