// runtime-debug.js — WebSocket 运行时黑板调试面板
// 协议（Game → Editor）：
//   { type:"bb_snapshot",   agent_id, agent_name, tree_name, tree_status,
//     properties:{k:v,...}, object_properties:{k:name,...} }
//   { type:"agent_removed", agent_id }
//   { type:"agent_tree_source", agent_id, agent_name, tree_name, source_path }
//   { type:"server_stopping" }
// 协议（Editor → Game）：
//   { type:"request_snapshot" }

'use strict';

class RuntimeDebugger {
    constructor() {
        this._ws             = null;
        this._isOpen         = false;
        this._host           = 'localhost';
        this._port           = 17654;
        this._isConnecting   = false;

        // Auto-connect behavior: try to connect every N ms when not connected
        this._autoConnectEnabled = true; // enabled by default
        this._autoConnectIntervalMs = 5000; // retry every 5 seconds
        this._autoConnectTimerId = null;

        // agentId → { name, treeName, treeStatus, properties, objectProperties, updatedAt }
        this._agents         = new Map();
        this._selectedId     = null;
        this._enableVerboseLogs = false;

        // DOM refs
        this._elPanel      = document.getElementById('debug-panel');
        this._elHeader     = document.getElementById('debug-panel-header');
        this._elContent    = document.getElementById('debug-content');
        this._elDot        = document.getElementById('ws-dot');
        this._elStatusLbl  = document.getElementById('ws-status-label');
        this._elHostInput  = document.getElementById('ws-host-input');
        this._elConnectBtn = document.getElementById('btn-ws-connect');
        this._elAgentSel   = document.getElementById('debug-agent-select');
        this._elTreeBadge  = document.getElementById('debug-tree-badge');
        this._elTbody      = document.querySelector('#debug-bb-table tbody');
        this._elThead      = document.querySelector('#debug-bb-table thead');

        this._debugLog('[RuntimeDebug] ctor: initial ws-host-input value =', this._elHostInput?.value);
        this._applyUrlAddressOverrides();
        this._debugLog('[RuntimeDebug] ctor: ws-host-input after URL override =', this._elHostInput?.value);
        this._bindUI();
        // 初始隐藏表头
        this._elThead.style.display = 'none';
        // start auto-connect loop
        if (this._autoConnectEnabled) this._startAutoConnect();
    }

    // ── UI 绑定 ──────────────────────────────────────────────────────────────

    _bindUI() {
        // 折叠/展开面板（点击 header 中的空白区域）
        this._elHeader.addEventListener('click', (e) => {
            if (e.target.closest('select,input,button')) return;
            this._elPanel.classList.toggle('collapsed');
            this._elPanel.classList.toggle('expanded');
        });

        // 连接 / 断开
        this._elConnectBtn.addEventListener('click', (e) => {
            e.stopPropagation();
            if (this._isOpen) {
                // user-initiated disconnect disables auto-connect until user clicks connect again
                this._autoConnectEnabled = false;
                this._stopAutoConnect();
                this.disconnect();
            } else {
                this._parseAddress(this._elHostInput.value.trim());
                // user-initiated connect re-enables auto-connect
                this._autoConnectEnabled = true;
                this._startAutoConnect();
                this.connect();
            }
        });

        // 阻止 input 点击折叠面板
        this._elHostInput.addEventListener('click',  (e) => e.stopPropagation());
        this._elHostInput.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') { e.stopPropagation(); this._elConnectBtn.click(); }
        });

        // Agent 切换
        this._elAgentSel.addEventListener('click',  (e) => e.stopPropagation());
        this._elAgentSel.addEventListener('change', () => {
            this._selectedId = this._elAgentSel.value || null;
            this._renderTable();
            this._updateDebugNodeMap();
        });

        // 拖拽调整面板高度
        const handle = document.getElementById('debug-resize-handle');
        let _resizing = false, _startY = 0, _startH = 0;
        handle.addEventListener('mousedown', (e) => {
            _resizing = true;
            _startY   = e.clientY;
            _startH   = this._elPanel.offsetHeight;
            e.preventDefault();
            e.stopPropagation();
        });
        document.addEventListener('mousemove', (e) => {
            if (!_resizing) return;
            // 向上拖动 = 增大高度
            const delta = _startY - e.clientY;
            const newH  = Math.max(30, Math.min(600, _startH + delta));
            this._elPanel.style.height = newH + 'px';
            if (newH <= 30) {
                this._elPanel.classList.add('collapsed');
                this._elPanel.classList.remove('expanded');
            } else {
                this._elPanel.classList.remove('collapsed');
                this._elPanel.classList.add('expanded');
            }
        });
        document.addEventListener('mouseup', () => { _resizing = false; });
    }

    _debugLog(...args) {
        if (!this._enableVerboseLogs) return;
        console.log(...args);
    }

    _keyLog(...args) {
        console.info(...args);
    }

    _parseAddress(raw) {
        this._debugLog('[RuntimeDebug] _parseAddress input =', raw);
        const str  = raw || 'localhost:17654';
        const idx  = str.lastIndexOf(':');
        if (idx > 0) {
            this._host = str.slice(0, idx);
            this._port = parseInt(str.slice(idx + 1), 10) || 17654;
        } else {
            this._host = str;
            this._port = 17654;
        }
        this._debugLog('[RuntimeDebug] _parseAddress result =', { host: this._host, port: this._port });
    }

    // ── WebSocket 连接 ───────────────────────────────────────────────────────

    _applyUrlAddressOverrides() {
        this._debugLog('[RuntimeDebug] _applyUrlAddressOverrides location.href =', window.location.href);
        this._debugLog('[RuntimeDebug] _applyUrlAddressOverrides location.search =', window.location.search);
        const params = new URLSearchParams(window.location.search);
        const host = (params.get('host') || '').trim();
        const port = (params.get('port') || '').trim();
        this._debugLog('[RuntimeDebug] _applyUrlAddressOverrides parsed params =', { host, port });
        if (!host && !port) {
            this._debugLog('[RuntimeDebug] _applyUrlAddressOverrides: no host/port query params found');
            return;
        }

        this._parseAddress(this._elHostInput.value.trim());

        if (host) this._host = host;
        if (port) this._port = parseInt(port, 10) || this._port || 17654;

        this._elHostInput.value = `${this._host}:${this._port}`;
        this._debugLog('[RuntimeDebug] _applyUrlAddressOverrides updated ws-host-input =', this._elHostInput.value);
    }

    connect() {
        if (this._ws) { try { this._ws.close(); } catch(_) {} this._ws = null; }

        const url = `ws://${this._host}:${this._port}`;
        this._setStatus('connecting', `正在连接 ${url}…`);
        this._elConnectBtn.textContent = '断开';

        try {
            this._isConnecting = true;
            this._ws = new WebSocket(url);
        } catch (e) {
            this._isConnecting = false;
            this._setStatus('error', `地址无效: ${e.message}`);
            this._elConnectBtn.textContent = '连接';
            return;
        }

        this._ws.addEventListener('open',    () => this._onOpen());
        this._ws.addEventListener('close',   (e) => this._onClose(e));
        this._ws.addEventListener('error',   ()  => this._setStatus('error', '连接出错'));
        this._ws.binaryType = 'arraybuffer'; // 统一以 ArrayBuffer 接收 binary frame
        this._ws.addEventListener('message', (e) => this._onMessage(e.data));
    }

    disconnect() {
        if (this._ws) { try { this._ws.close(); } catch(_) {} this._ws = null; }
        this._isOpen = false;
        this._setStatus('disconnected', '已断开');
        this._elConnectBtn.textContent = '连接';
        this._clearRuntimeState();
    }

    _clearRuntimeState() {
        this._agents.clear();
        this._selectedId = null;
        this._syncSelector();
        this._renderTable();
        this._updateDebugNodeMap();
    }

    _onOpen() {
        this._isOpen = true;
        this._isConnecting = false;

        // 清空旧的 Agent 列表，防止与新会话的数据混淆
        this._clearRuntimeState();

        this._setStatus('connected', `ws://${this._host}:${this._port}`);
        this._keyLog('[RuntimeDebug] Connected:', `ws://${this._host}:${this._port}`);
        this._elConnectBtn.textContent = '断开';
        // 展开面板（首次连接时）
        if (this._elPanel.classList.contains('collapsed')) {
            this._elPanel.classList.remove('collapsed');
            this._elPanel.classList.add('expanded');
        }
        // 请求一次全量快照
        this._send({ type: 'request_snapshot' });
    }

    _onClose(e) {
        this._isOpen = false;
        this._ws     = null;
        this._isConnecting = false;
        const reason = e.reason ? ` (${e.reason})` : '';
        this._setStatus('disconnected', `连接已关闭${reason}`);
        this._keyLog('[RuntimeDebug] Disconnected', reason || '');
        this._elConnectBtn.textContent = '连接';
        this._clearRuntimeState();
    }

    // ── Auto-connect helpers ─────────────────────────────────────────────

    _startAutoConnect() {
        if (this._autoConnectTimerId) return;
        // immediate attempt
        if (!this._isOpen && !this._isConnecting) {
            this._debugLog('[RuntimeDebug] _startAutoConnect immediate attempt with input =', this._elHostInput.value);
            this._parseAddress(this._elHostInput.value.trim());
            this.connect();
        }
        this._autoConnectTimerId = setInterval(() => {
            if (!this._autoConnectEnabled) return;
            if (this._isOpen || this._isConnecting) return;
            this._debugLog('[RuntimeDebug] _startAutoConnect retry attempt with input =', this._elHostInput.value);
            this._parseAddress(this._elHostInput.value.trim());
            this.connect();
        }, this._autoConnectIntervalMs);
    }

    _stopAutoConnect() {
        if (this._autoConnectTimerId) {
            clearInterval(this._autoConnectTimerId);
            this._autoConnectTimerId = null;
        }
    }

    // ── 消息处理 ─────────────────────────────────────────────────────────────

    // UE 的 INetworkingWebSocket::Send 默认发送 binary frame，
    // 浏览器接收为 Blob 或 ArrayBuffer。此处统一转为字符串再分发。
    _onMessage(rawData) {
        if (typeof rawData === 'string') {
            this._debugLog('[RuntimeDebug][消息] string 类型，长度:', rawData.length);
            this._dispatch(rawData);
        } else if (rawData instanceof Blob) {
            this._debugLog('[RuntimeDebug][消息] Blob 类型，size:', rawData.size);
            rawData.text().then(s => this._dispatch(s));
        } else if (rawData instanceof ArrayBuffer) {
            this._debugLog('[RuntimeDebug][消息] ArrayBuffer 类型，byteLength:', rawData.byteLength);
            this._dispatch(new TextDecoder().decode(rawData));
        } else {
            console.warn('[RuntimeDebug] 未知消息类型:', typeof rawData);
        }
    }

    _dispatch(raw) {
        let msg;
        try { msg = JSON.parse(raw); }
        catch (e) {
            console.warn('[RuntimeDebug] JSON 解析失败，原始数据(前200字符):', raw.slice(0, 200), '错误:', e.message);
            return;
        }
        this._debugLog('[RuntimeDebug][dispatch] type:', msg.type);
        switch (msg.type) {
            case 'bb_snapshot':    this._handleSnapshot(msg);                        break;
            case 'agent_removed':  this._handleAgentRemoved(String(msg.agent_id));   break;
            case 'agent_tree_source': this._handleAgentTreeSource(msg);              break;
            case 'server_stopping': this._handleServerStopping();                    break;
            default:
            console.warn('[RuntimeDebug] 未知消息类型:', msg.type, msg);
        }
    }

    _handleServerStopping() {
        this._keyLog('[RuntimeDebug] Received server_stopping');
        this._clearRuntimeState();
        this._isOpen = false;
        this._isConnecting = false;
        this._setStatus('disconnected', '服务器正在停止');
        this._elConnectBtn.textContent = '连接';
    }

    _handleSnapshot(msg) {
        const id = String(msg.agent_id ?? '');
        if (!id) { console.warn('[RuntimeDebug] bb_snapshot 缺少 agent_id'); return; }

        const prev = this._agents.get(id);

        this._agents.set(id, {
            name:             String(msg.agent_name  || id),
            treeName:         String(msg.tree_name   || ''),
            treeStatus:       String(msg.tree_status || 'Invalid'),
            properties:       (msg.properties        && typeof msg.properties        === 'object') ? msg.properties        : {},
            objectProperties: (msg.object_properties && typeof msg.object_properties === 'object') ? msg.object_properties : {},
            nodes:            Array.isArray(msg.nodes) ? msg.nodes : [],
            sourcePath:       prev?.sourcePath || '',
            updatedAt:        Date.now(),
        });

        // 若当前无选中 agent，自动选此 agent
        if (!this._selectedId) this._selectedId = id;

        this._syncSelector();

        // 仅当前选中的 agent 发生变化时重绘
        if (this._selectedId === id) {
            this._renderTable();
            this._updateDebugNodeMap();
        }
    }

    _handleAgentRemoved(id) {
        this._agents.delete(id);
        if (this._selectedId === id) {
            this._selectedId = null;
            this._updateDebugNodeMap(); // 清空画布发光
        }
        this._syncSelector();
        this._renderTable();
    }

    _handleAgentTreeSource(msg) {
        const id = String(msg.agent_id || '');
        const treeName = String(msg.tree_name || '');
        const sourcePath = String(msg.source_path || msg.path || '');
        const agentName = String(msg.agent_name || id || '(unknown)');
        this._debugLog('[RuntimeDebug][TreePath] 收到 agent_tree_source，agent:', agentName, 'tree_name:', treeName, 'path:', sourcePath);

        if (!id || !treeName || !sourcePath) {
            console.warn('[RuntimeDebug][TreePath] agent_id、tree_name 或 source_path 为空，跳过。', msg);
            return;
        }

        const prev = this._agents.get(id);
        this._agents.set(id, {
            name:             agentName,
            treeName:         treeName,
            treeStatus:       prev?.treeStatus || 'Invalid',
            properties:       prev?.properties || {},
            objectProperties: prev?.objectProperties || {},
            nodes:            prev?.nodes || [],
            sourcePath:       sourcePath,
            updatedAt:        prev?.updatedAt || Date.now(),
        });

        if (this._selectedId === id) {
            this._renderTable();
        }

        if (typeof activeGraph === 'function') {
            const graph = activeGraph();
            if (graph && String(graph.treeName || '') === treeName) {
                graph.sourcePath = sourcePath;
            }
        }
    }

    // ── UI 更新 ──────────────────────────────────────────────────────────────

    _syncSelector() {
        const sel  = this._elAgentSel;
        const prev = sel.value;
        sel.innerHTML = '';

        if (this._agents.size === 0) {
            const opt = document.createElement('option');
            opt.value = '';
            opt.textContent = '（无 Agent）';
            sel.appendChild(opt);
            this._selectedId = null;
            return;
        }

        for (const [id, data] of this._agents) {
            const opt = document.createElement('option');
            opt.value       = id;
            opt.textContent = data.name;
            opt.title       = data.sourcePath || '';
            sel.appendChild(opt);
        }

        // 尽量保持之前的选中项
        if (prev && this._agents.has(prev)) {
            sel.value        = prev;
            this._selectedId = prev;
        } else {
            const firstId    = this._agents.keys().next().value;
            sel.value        = firstId;
            this._selectedId = firstId;
        }
    }

    _renderTable() {
        this._elTbody.innerHTML = '';

        if (!this._selectedId) {
            this._elThead.style.display = 'none';
            this._elTreeBadge.textContent = '';
            this._elTreeBadge.className   = 'debug-tree-badge';
            return;
        }

        const data = this._agents.get(this._selectedId);
        if (!data) { this._elThead.style.display = 'none'; return; }

        this._elThead.style.display = '';

        // 树名 + 状态徽章
        const statusKey = (data.treeStatus || 'invalid').toLowerCase();
        this._elTreeBadge.textContent = data.treeName
            ? `${data.treeName}  ·  ${data.treeStatus}`
            : data.treeStatus;
        this._elTreeBadge.className = `debug-tree-badge status-${statusKey}`;
        this._elTreeBadge.title = data.sourcePath || '';

        // 合并两类属性并排序
        const rows = [];
        for (const [k, v] of Object.entries(data.properties)) {
            rows.push({ key: k, raw: String(v), isObj: false, display: String(v) });
        }
        for (const [k, v] of Object.entries(data.objectProperties)) {
            const display = v ? String(v) : '(null)';
            rows.push({ key: k, raw: display, isObj: true, display });
        }
        rows.sort((a, b) => a.key.localeCompare(b.key));

        if (rows.length === 0) {
            const tr = document.createElement('tr');
            tr.innerHTML = `<td colspan="3" class="bb-empty">（黑板为空）</td>`;
            this._elTbody.appendChild(tr);
            return;
        }

        const frag = document.createDocumentFragment();
        for (const row of rows) {
            const tr  = document.createElement('tr');
            const typeBadge = row.isObj
                ? `<span class="bb-type-badge obj">UObj</span>`
                : `<span class="bb-type-badge val">Val</span>`;
            tr.innerHTML = `
                <td class="bb-key">${_esc(row.key)}</td>
                <td class="bb-val${row.isObj ? ' bb-obj' : ''}">${_esc(row.display)}</td>
                <td class="bb-type">${typeBadge}</td>`;
            frag.appendChild(tr);
        }
        this._elTbody.appendChild(frag);
    }

    // ── 调试节点映射（供 graph.js 画布发光使用）────────────────────────────────

    /** 将当前选中 agent 的节点状态同步到 window._behaviacDebugNodes
     *  graph.js 的 _drawNode() 通过该 Map 读取每个节点的运行时状态
     */
    _updateDebugNodeMap() {
        if (!window._behaviacDebugNodes) {
            window._behaviacDebugNodes = new Map();
        }
        window._behaviacDebugNodes.clear();

        if (!this._selectedId) return;
        const data = this._agents.get(this._selectedId);
        if (!data || !data.nodes.length) return;

        // 仅当画布中当前渲染的行为树路径与选中 Agent 的路径一致时才高亮节点；
        // 路径不匹配说明画布显示的是另一棵树，强行高亮节点 ID 会造成误导。
        if (typeof activeGraph === 'function') {
            const graph = activeGraph();
            const canvasPath = String(graph?.sourcePath || '');
            const agentPath  = String(data.sourcePath   || '');
            const normalizedCanvasPath = canvasPath.trim().replace(/\\/g, '/').toLowerCase();
            const normalizedAgentPath  = agentPath.trim().replace(/\\/g, '/').toLowerCase();
            if (normalizedCanvasPath && normalizedAgentPath && normalizedCanvasPath !== normalizedAgentPath) {
                return; // 路径不匹配，不写入调试状态
            }
        }

        for (const n of data.nodes) {
            // n.id 与编辑器图节点 n.id 均来自 XML id 属性，可直接匹配
            window._behaviacDebugNodes.set(n.id, String(n.status || 'Invalid'));
        }
    }

    _setStatus(state, msg) {
        this._elDot.className       = `ws-dot status-${state}`;
        this._elDot.title           = msg;
        this._elStatusLbl.textContent = msg;
    }

    _send(obj) {
        if (this._ws && this._ws.readyState === WebSocket.OPEN) {
            this._ws.send(JSON.stringify(obj));
        }
    }
}

// ── 工具函数 ─────────────────────────────────────────────────────────────────

function _esc(s) {
    return String(s)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;');
}

// ── 初始化（等待 DOM 就绪）────────────────────────────────────────────────────

document.addEventListener('DOMContentLoaded', () => {
    window.runtimeDebugger = new RuntimeDebugger();
});