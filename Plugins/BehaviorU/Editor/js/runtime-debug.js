// runtime-debug.js — Runtime blackboard debug client for BehaviorU.
//
// Connects to the UE-side behavior-tree debug WebSocket server (default port 17654,
// see FBehaviacDebugServer) and drives the "Runtime Blackboard" panel in index.html:
//   • ws-host-input / btn-ws-connect / ws-dot / ws-status-label — connection controls
//   • debug-agent-select  — per-agent dropdown
//   • debug-tree-badge    — selected agent's tree name + status
//   • debug-bb-table      — live blackboard properties (key / value / type)
//
// Wire protocol (UTF-8 JSON), per BehaviacDebugServer.h/.cpp:
//   Editor → Game : a text frame containing "request_snapshot" (the server substring-
//                  matches it) asks for an immediate snapshot.
//   Game  → Editor:
//     bb_snapshot        { type, agent_id, agent_name, tree_name, tree_status,
//                          properties:{k:v}, object_properties:{k:v},
//                          nodes:[{id,class,status}] }
//     agent_removed      { type, agent_id }
//     agent_tree_source  { type, agent_id, agent_name, tree_name, source_path }
//
// This file is loaded after the rest of the editor JS, at the end of <body>. It is
// self-contained and never throws if the panel elements are missing.

(function () {
  'use strict';

  const TAG = '[BehaviacEditor][RuntimeDebug]';

  // ── Element helpers (null-safe) ─────────────────────────
  const $ = (id) => document.getElementById(id);

  // ── Connection state ────────────────────────────────────
  let ws = null;
  let manuallyClosed = false;

  // agent_id -> { agent_name, tree_name, tree_status, properties, object_properties,
  //               nodes, source_path }
  const agents = new Map();
  let selectedAgentId = null;

  // ── UI binding ───────────────────────────────────────────
  let hostInput, connectBtn, dot, statusLabel, agentSelect, treeBadge, tableBody;
  let panel, panelHeader, resizeHandle;
  let autoExpandedOnce = false;

  function bindEls() {
    hostInput    = $('ws-host-input');
    connectBtn   = $('btn-ws-connect');
    dot          = $('ws-dot');
    statusLabel  = $('ws-status-label');
    agentSelect  = $('debug-agent-select');
    treeBadge    = $('debug-tree-badge');
    const table  = $('debug-bb-table');
    tableBody    = table ? table.querySelector('tbody') : null;
    panel        = $('debug-panel');
    panelHeader  = $('debug-panel-header');
    resizeHandle = $('debug-resize-handle');
  }

  // ── Panel expand / collapse / resize ─────────────────────
  function expandPanel() {
    if (panel) panel.classList.add('expanded');
  }

  function togglePanel() {
    if (panel) panel.classList.toggle('expanded');
  }

  function wirePanelControls() {
    // Click the header bar to expand/collapse the blackboard panel.
    // Ignore clicks that originate from the agent <select> so choosing an
    // agent doesn't collapse the panel.
    if (panelHeader && panel) {
      panelHeader.addEventListener('click', (e) => {
        if (e.target && e.target.closest('#debug-agent-select')) return;
        togglePanel();
      });
    }

    // Drag the top resize handle to set an explicit height.
    if (resizeHandle && panel) {
      resizeHandle.addEventListener('mousedown', (e) => {
        e.preventDefault();
        e.stopPropagation(); // don't let it also toggle the header
        const startY  = e.clientY;
        const startH  = panel.getBoundingClientRect().height;
        const onMove  = (ev) => {
          const dy = startY - ev.clientY;   // drag up → taller
          let h = Math.round(startH + dy);
          h = Math.max(60, Math.min(600, h));
          panel.style.height = h + 'px';
          panel.classList.add('expanded');
        };
        const onUp = () => {
          window.removeEventListener('mousemove', onMove);
          window.removeEventListener('mouseup', onUp);
        };
        window.addEventListener('mousemove', onMove);
        window.addEventListener('mouseup', onUp);
      });
    }
  }

  // ── Visual state ────────────────────────────────────────
  function setDot(state) {
    if (!dot) return;
    dot.classList.remove('status-disconnected', 'status-connected', 'status-connecting', 'status-error');
    dot.classList.add('status-' + state);
  }

  function setLabel(text, btnText) {
    if (statusLabel) statusLabel.textContent = text;
    if (connectBtn && btnText) connectBtn.textContent = btnText;
  }

  function resetUI() {
    setDot('disconnected');
    setLabel('未连接', '连接');
  }

  // ── Host/port parsing ────────────────────────────────────
  function parseHostPort() {
    const raw = (hostInput ? hostInput.value : '') || 'localhost:17654';
    const idx = raw.lastIndexOf(':');
    let host, port;
    if (idx > 0) { host = raw.slice(0, idx); port = raw.slice(idx + 1); }
    else        { host = raw; port = '17654'; }
    host = host.trim() || 'localhost';
    port = (port.trim() || '17654');
    return { host, port };
  }

  // ── Connect / disconnect ────────────────────────────────
  function connect() {
    if (ws) disconnect();
    if (!hostInput) { console.warn(TAG, 'ws-host-input not found'); return; }

    const { host, port } = parseHostPort();
    const url = 'ws://' + host + ':' + port;
    manuallyClosed = false;

    setDot('connecting');
    setLabel('连接中…', '断开');
    console.log(TAG, 'connecting', url);

    try {
      ws = new WebSocket(url);
    } catch (e) {
      setDot('error');
      setLabel('错误', '连接');
      console.error(TAG, 'WebSocket construction failed:', e);
      ws = null;
      return;
    }

    ws.onopen = () => {
      setDot('connected');
      setLabel('已连接', '断开');
      console.log(TAG, 'connected');
      send({ type: 'request_snapshot' });
    };

    ws.onmessage = (ev) => {
      let msg;
      try { msg = JSON.parse(ev.data); } catch (e) { return; }  // ignore non-JSON frames
      try { handleMessage(msg); } catch (e) { console.error(TAG, 'handler error:', e); }
    };

    ws.onerror = (e) => {
      console.error(TAG, 'socket error', e);
      setDot('error');
      setLabel('错误', '连接');
    };

    ws.onclose = () => {
      console.log(TAG, 'disconnected');
      ws = null;
      if (!manuallyClosed) {
        // unexpected close — show as disconnected but keep the button ready to retry
        setDot('disconnected');
        setLabel('未连接', '连接');
      } else {
        resetUI();
      }
    };
  }

  function disconnect() {
    manuallyClosed = true;
    if (ws) {
      try { ws.close(); } catch (e) {}
      ws = null;
    }
    resetUI();
  }

  function send(obj) {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(obj));
      return true;
    }
    return false;
  }

  // ── Message handling ────────────────────────────────────
  function handleMessage(msg) {
    switch (msg.type) {
      case 'bb_snapshot':       onSnapshot(msg); break;
      case 'agent_removed':     onAgentRemoved(msg); break;
      case 'agent_tree_source': onTreeSource(msg); break;
      default: /* ignore unknown */ break;
    }
  }

  function onSnapshot(msg) {
    const id = String(msg.agent_id);
    const existing = agents.get(id) || {};
    const agent = {
      agent_id:        id,
      agent_name:      msg.agent_name ?? existing.agent_name ?? '',
      tree_name:       msg.tree_name ?? existing.tree_name ?? '',
      tree_status:    msg.tree_status ?? existing.tree_status ?? '',
      properties:      msg.properties ?? existing.properties ?? {},
      object_properties: msg.object_properties ?? existing.object_properties ?? {},
      nodes:           msg.nodes ?? existing.nodes ?? [],
      source_path:     existing.source_path ?? '',
    };
    agents.set(id, agent);
    rebuildAgentSelect();
    if (id === selectedAgentId || selectedAgentId === null) {
      selectedAgentId = id;
      if (agentSelect) agentSelect.value = id;
      renderAgent(agent);
    }

    // Auto-expand the panel the first time a snapshot with real blackboard
    // data arrives, so the user sees the table without having to click.
    if (!autoExpandedOnce && agent.properties &&
        Object.keys(agent.properties).length > 0) {
      expandPanel();
      autoExpandedOnce = true;
    }
  }

  function onAgentRemoved(msg) {
    const id = String(msg.agent_id);
    agents.delete(id);
    rebuildAgentSelect();
    if (id === selectedAgentId) {
      selectedAgentId = null;
      if (agentSelect && agentSelect.options.length) {
        selectedAgentId = agentSelect.options[0].value;
        agentSelect.value = selectedAgentId;
      }
      renderAgent(selectedAgentId ? agents.get(selectedAgentId) : null);
    }
  }

  function onTreeSource(msg) {
    const id = String(msg.agent_id);
    const agent = agents.get(id);
    if (agent) {
      agent.source_path = msg.source_path || '';
      if (msg.agent_name) agent.agent_name = msg.agent_name;
      if (msg.tree_name)  agent.tree_name  = msg.tree_name;
      rebuildAgentSelect();
      if (id === selectedAgentId) renderAgent(agent);
    }
  }

  // ── Dropdown ─────────────────────────────────────────────
  function rebuildAgentSelect() {
    if (!agentSelect) return;
    const prev = selectedAgentId;
    const ids = [...agents.keys()];
    // Preserve the current selection when it still exists.
    agentSelect.innerHTML = '';
    for (const id of ids) {
      const a = agents.get(id);
      const opt = document.createElement('option');
      opt.value = id;
      opt.textContent = a.agent_name ? (a.agent_name + '  (#' + id + ')') : ('Agent #' + id);
      agentSelect.appendChild(opt);
    }
    if (ids.length && (prev === null || !ids.includes(prev))) {
      selectedAgentId = ids[0];
    } else if (!ids.length) {
      selectedAgentId = null;
    } else {
      selectedAgentId = prev;
    }
    if (selectedAgentId !== null) agentSelect.value = selectedAgentId;
  }

  // ── Rendering ────────────────────────────────────────────
  function renderAgent(agent) {
    // tree badge
    if (treeBadge) {
      if (agent) {
        const parts = [];
        if (agent.tree_name)    parts.push(agent.tree_name);
        if (agent.tree_status)  parts.push('[' + agent.tree_status + ']');
        treeBadge.textContent = parts.join(' ');
      } else {
        treeBadge.textContent = '';
      }
    }

    if (!tableBody) return;
    tableBody.innerHTML = '';

    if (!agent) return;

    const rows = [];
    const pushRows = (obj, isObject) => {
      if (!obj) return;
      for (const [k, v] of Object.entries(obj)) {
        rows.push([k, v, isObject ? 'object' : inferType(v)]);
      }
    };
    pushRows(agent.properties, false);
    pushRows(agent.object_properties, true);

    if (!rows.length) {
      const tr = document.createElement('tr');
      const td = document.createElement('td');
      td.colSpan = 3;
      td.style.cssText = 'color:var(--text-muted);text-align:center;padding:12px;';
      td.textContent = '(no blackboard properties)';
      tr.appendChild(td);
      tableBody.appendChild(tr);
      return;
    }

    for (const [key, val, type] of rows) {
      const tr = document.createElement('tr');
      tr.appendChild(cell(key));
      tr.appendChild(cell(val));
      tr.appendChild(cell(type));
      tableBody.appendChild(tr);
    }
  }

  function cell(text) {
    const td = document.createElement('td');
    td.textContent = (text === undefined || text === null) ? '' : String(text);
    return td;
  }

  // Best-effort type label from a value string (server sends everything as strings).
  function inferType(v) {
    if (v === true || v === false) return 'bool';
    const s = String(v).trim();
    if (!s) return '';
    if (/^(true|false)$/i.test(s)) return 'bool';
    if (/^-?\d+(\.\d+)?$/.test(s))  return 'number';
    if (/^[(-]?-?\d+(\.\d+)?(,\s*-?\d+(\.\d+)?){1,3}[)]?$/.test(s)) return 'vector/struct';
    return 'string';
  }

  // ── Init ────────────────────────────────────────────────
  function init() {
    bindEls();
    if (!connectBtn || !hostInput) {
      console.warn(TAG, 'debug panel controls not found; runtime-debug disabled');
      return;
    }
    connectBtn.addEventListener('click', () => {
      if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) {
        disconnect();
      } else {
        connect();
      }
    });
    if (agentSelect) {
      agentSelect.addEventListener('change', () => {
        selectedAgentId = agentSelect.value || null;
        renderAgent(selectedAgentId ? agents.get(selectedAgentId) : null);
      });
    }
    wirePanelControls();
    resetUI();
    console.log(TAG, 'ready (target', parseHostPort().host + ':' + parseHostPort().port + ')');
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }

  // Expose a minimal handle for debugging from the console / other scripts.
  window.BehaviacDebug = {
    connect, disconnect, requestSnapshot: () => send({ type: 'request_snapshot' }),
    get agents() { return agents; },
  };
})();