// app.js — Main application entry point

// ── State ────────────────────────────────────────────────
let _nextId  = 1;
let _tabs    = [];       // [{id, name, graph}]
let _activeTab = null;  // tab id

function newGraph(name) {
  return { treeName: name || 'BT_Unnamed', nodes: [], edges: [], selectedIds: new Set(), selectedEdge: null, dragEdge: null };
}

function activeGraph() { return _tabs.find(t => t.id === _activeTab)?.graph || newGraph(); }

// ── Renderer + subsystems ────────────────────────────────
let renderer, propsPanel, undoStack;

// ── Interaction state ────────────────────────────────────
const interact = {
  isPanning:   false,
  panStart:    { mx: 0, my: 0, cx: 0, cy: 0 },
  isDraggingNodes: false,
  dragNodeStart: null,   // [{id, ox, oy}]
  isDraggingEdge:  false,
  dragEdgeFromId: null,
  isSelBox:    false,
  selBoxStart: { wx: 0, wy: 0 },
  spaceDown:   false,
};

// ── Bootstrap ────────────────────────────────────────────
document.addEventListener('DOMContentLoaded', () => {
  const canvas = document.getElementById('main-canvas');
  const wrap   = document.getElementById('canvas-wrap');

  renderer   = new GraphRenderer(canvas, activeGraph);
  propsPanel = new PropertiesPanel(document.getElementById('props-content'), (nodeId, _props) => {
    undoStack.push(snapshot());
    renderer.markDirty();
    renderMinimap();
    updateStatus();
  });
  undoStack  = new UndoStack(50);

  // First tab
  addTab('BT_Unnamed');

  // Resize observer
  const ro = new ResizeObserver(() => {
    renderer.resize(wrap.clientWidth, wrap.clientHeight);
    renderMinimap();
  });
  ro.observe(wrap);
  renderer.resize(wrap.clientWidth, wrap.clientHeight);

  // Push initial snapshot
  undoStack.push(snapshot());

  buildPalette();
  bindEvents(canvas, wrap);
  bindToolbar();
  bindKeyboard();
  buildMinimap();
  updateStatus();
});

// ── Tab management ───────────────────────────────────────
function addTab(name) {
  const id = 'tab_' + (++_nextId);
  const graph = newGraph(name || 'BT_Unnamed');
  _tabs.push({ id, name: graph.treeName, graph });
  setActiveTab(id);
  renderTabs();
}

function setActiveTab(id) {
  _activeTab = id;
  const tab = _tabs.find(t => t.id === id);
  if (tab) {
    document.getElementById('tree-name-input').value = tab.graph.treeName;
    propsPanel.clear();
    renderer.markDirty();
    renderMinimap();
    updateStatus();
  }
  renderTabs();
}

function closeTab(id) {
  if (_tabs.length <= 1) return;
  const idx = _tabs.findIndex(t => t.id === id);
  _tabs.splice(idx, 1);
  if (_activeTab === id) setActiveTab(_tabs[Math.min(idx, _tabs.length-1)].id);
  renderTabs();
}

function renderTabs() {
  const bar = document.getElementById('tab-bar');
  const addBtn = document.getElementById('tab-add');
  // clear existing tabs (keep addBtn)
  [...bar.querySelectorAll('.tab')].forEach(el => el.remove());
  for (const t of _tabs) {
    const el = document.createElement('div');
    el.className = 'tab' + (t.id === _activeTab ? ' active' : '');
    el.dataset.id = t.id;
    el.innerHTML = `<span class="tab-label">${escHtml(t.name)}</span><span class="tab-close">✕</span>`;
    el.querySelector('.tab-close').addEventListener('click', e => { e.stopPropagation(); closeTab(t.id); });
    el.addEventListener('click', () => setActiveTab(t.id));
    bar.insertBefore(el, addBtn);
  }
}

// ── Palette ──────────────────────────────────────────────
function buildPalette() {
  const list   = document.getElementById('palette-list');
  const search = document.getElementById('palette-search');
  const groups = {};
  for (const n of NODE_TYPES) {
    if (n.type === '__Root__') continue;
    if (!groups[n.category]) groups[n.category] = [];
    groups[n.category].push(n);
  }
  const catOrder = ['composite','action','condition','decorator','attachment'];
  const catLabel = { composite:'Composites', action:'Actions', condition:'Conditions', decorator:'Decorators', attachment:'Attachments' };

  function render(filter) {
    list.innerHTML = '';
    for (const cat of catOrder) {
      const items = (groups[cat] || []).filter(n => !filter || n.type.toLowerCase().includes(filter));
      if (!items.length) continue;
      const catDiv = document.createElement('div');
      catDiv.className = 'palette-category';
      const header = document.createElement('div');
      header.className = 'palette-cat-header';
      header.innerHTML = `▾ ${catLabel[cat] || cat}`;
      catDiv.appendChild(header);
      const body = document.createElement('div');
      body.className = 'palette-cat-body';
      for (const n of items) {
        const item = document.createElement('div');
        item.className = `palette-item cat-${cat}`;
        item.textContent = n.type;
        item.title = n.tooltip || '';
        item.draggable = true;
        item.dataset.type = n.type;
        item.addEventListener('dragstart', e => {
          e.dataTransfer.setData('text/plain', n.type);
          e.dataTransfer.effectAllowed = 'copy';
        });
        body.appendChild(item);
      }
      catDiv.appendChild(body);
      // collapse toggle
      header.addEventListener('click', () => {
        body.style.display = body.style.display === 'none' ? '' : 'none';
        header.textContent = (body.style.display === 'none' ? '▸ ' : '▾ ') + (catLabel[cat]||cat);
      });
      list.appendChild(catDiv);
    }
  }
  render('');
  search.addEventListener('input', () => render(search.value.trim().toLowerCase()));
}

// ── Canvas events ────────────────────────────────────────
function bindEvents(canvas, wrap) {
  // drag from palette
  wrap.addEventListener('dragover', e => { e.preventDefault(); e.dataTransfer.dropEffect = 'copy'; });
  wrap.addEventListener('drop', e => {
    e.preventDefault();
    const type = e.dataTransfer.getData('text/plain');
    if (!type) return;
    const rect = wrap.getBoundingClientRect();
    const sx = e.clientX - rect.left;
    const sy = e.clientY - rect.top;
    const {x, y} = renderer.toWorld(sx, sy);
    createNode(type, x - NODE_W/2, y - NODE_H/2);
  });

  canvas.addEventListener('mousedown', onMouseDown);
  canvas.addEventListener('mousemove', onMouseMove);
  canvas.addEventListener('mouseup',   onMouseUp);
  canvas.addEventListener('mouseleave', onMouseUp);
  canvas.addEventListener('dblclick',  onDblClick);
  canvas.addEventListener('contextmenu', onContextMenu);
  canvas.addEventListener('wheel', e => {
    e.preventDefault();
    const rect = canvas.getBoundingClientRect();
    const mx = e.clientX - rect.left;
    const my = e.clientY - rect.top;
    const zoomFactor = e.deltaY < 0 ? 1.1 : 0.9;
    const oldZoom = renderer.camera.zoom;
    const newZoom = Math.max(0.15, Math.min(3, oldZoom * zoomFactor));
    // zoom toward mouse
    renderer.camera.x -= mx/oldZoom - mx/newZoom;
    renderer.camera.y -= my/oldZoom - my/newZoom;
    renderer.camera.zoom = newZoom;
    renderer.markDirty();
    updateStatus();
    renderMinimap();
  }, { passive: false });
}

function worldMouse(e) {
  const rect = renderer.canvas.getBoundingClientRect();
  return renderer.toWorld(e.clientX - rect.left, e.clientY - rect.top);
}

function onMouseDown(e) {
  hideCtxMenu();
  const g = activeGraph();
  const { x, y } = worldMouse(e);

  if (e.button === 1 || (e.button === 0 && interact.spaceDown)) {
    // pan
    interact.isPanning = true;
    interact.panStart  = { mx: e.clientX, my: e.clientY, cx: renderer.camera.x, cy: renderer.camera.y };
    return;
  }

  if (e.button === 0) {
    // Check output port first → start edge drag
    const portNode = renderer.hitOutputPort(x, y, g.nodes);
    if (portNode) {
      interact.isDraggingEdge = true;
      interact.dragEdgeFromId = portNode.id;
      g.dragEdge = { fromId: portNode.id, x, y };
      renderer.markDirty();
      return;
    }

    // hit node
    const hit = renderer.hitNode(x, y, g.nodes);
    if (hit) {
      // select
      if (!e.shiftKey && !g.selectedIds.has(hit.id)) {
        g.selectedIds = new Set([hit.id]);
        g.selectedEdge = null;
        propsPanel.show(hit);
      } else if (e.shiftKey) {
        if (g.selectedIds.has(hit.id)) g.selectedIds.delete(hit.id);
        else g.selectedIds.add(hit.id);
        propsPanel.show(g.selectedIds.size === 1 ? g.nodes.find(n => g.selectedIds.has(n.id)) : null);
      }
      // start drag
      interact.isDraggingNodes = true;
      interact.dragNodeStart   = [...g.selectedIds].map(id => {
        const n = g.nodes.find(nn => nn.id === id);
        return { id, ox: n.x - x, oy: n.y - y };
      });
      renderer.markDirty();
      return;
    }

    // hit edge
    const edge = renderer.hitEdge(x, y, g.edges, g.nodes);
    if (edge) {
      g.selectedIds  = new Set();
      g.selectedEdge = edge;
      propsPanel.clear();
      renderer.markDirty();
      return;
    }

    // empty space → selection box or deselect
    g.selectedIds  = new Set();
    g.selectedEdge = null;
    propsPanel.clear();
    interact.isSelBox  = true;
    interact.selBoxStart = { wx: x, wy: y };
    renderer.markDirty();
  }
}

function onMouseMove(e) {
  const g = activeGraph();
  const { x, y } = worldMouse(e);

  if (interact.isPanning) {
    const dx = e.clientX - interact.panStart.mx;
    const dy = e.clientY - interact.panStart.my;
    renderer.camera.x = interact.panStart.cx + dx / renderer.camera.zoom;
    renderer.camera.y = interact.panStart.cy + dy / renderer.camera.zoom;
    renderer.markDirty();
    renderMinimap();
    return;
  }

  if (interact.isDraggingEdge) {
    if (g.dragEdge) { g.dragEdge.x = x; g.dragEdge.y = y; }
    renderer.markDirty();
    return;
  }

  if (interact.isDraggingNodes && interact.dragNodeStart) {
    for (const { id, ox, oy } of interact.dragNodeStart) {
      const n = g.nodes.find(nn => nn.id === id);
      if (n) { n.x = x + ox; n.y = y + oy; }
    }
    renderer.markDirty();
    renderMinimap();
    return;
  }

  if (interact.isSelBox) {
    const { wx, wy } = interact.selBoxStart;
    const minX = Math.min(wx, x), minY = Math.min(wy, y);
    const maxX = Math.max(wx, x), maxY = Math.max(wy, y);
    // screen coords for the box overlay
    const sMin = renderer.toScreen(minX, minY);
    const sMax = renderer.toScreen(maxX, maxY);
    const box  = document.getElementById('sel-box');
    box.style.display = 'block';
    box.style.left   = sMin.x + 'px'; box.style.top    = sMin.y + 'px';
    box.style.width  = (sMax.x - sMin.x) + 'px'; box.style.height = (sMax.y - sMin.y) + 'px';

    // select nodes in box
    g.selectedIds = new Set(g.nodes
      .filter(n => n.x+NODE_W > minX && n.x < maxX && n.y+NODE_H > minY && n.y < maxY)
      .map(n => n.id));
    renderer.markDirty();
    return;
  }
}

function onMouseUp(e) {
  const g = activeGraph();
  const { x, y } = worldMouse(e);

  if (interact.isPanning) { interact.isPanning = false; return; }

  if (interact.isDraggingEdge) {
    interact.isDraggingEdge = false;
    g.dragEdge = null;
    const targetNode = renderer.hitInputPort(x, y, g.nodes);
    if (targetNode && targetNode.id !== interact.dragEdgeFromId) {
      // check for duplicate
      const exists = g.edges.some(e2 => e2.from === interact.dragEdgeFromId && e2.to === targetNode.id);
      if (!exists) {
        g.edges.push({ from: interact.dragEdgeFromId, to: targetNode.id });
        undoStack.push(snapshot());
        updateStatus();
      }
    }
    interact.dragEdgeFromId = null;
    renderer.markDirty();
    return;
  }

  if (interact.isDraggingNodes) {
    interact.isDraggingNodes = false;
    interact.dragNodeStart   = null;
    undoStack.push(snapshot());
    renderer.markDirty();
    renderMinimap();
    return;
  }

  if (interact.isSelBox) {
    interact.isSelBox = false;
    document.getElementById('sel-box').style.display = 'none';
    renderer.markDirty();
    return;
  }
}

function onDblClick(e) {
  const g  = activeGraph();
  const { x, y } = worldMouse(e);
  const hit = renderer.hitNode(x, y, g.nodes);
  if (hit) {
    // inline rename
    const label = prompt('Rename node:', hit.label || hit.type);
    if (label !== null) {
      hit.label = label;
      propsPanel.show(hit);
      undoStack.push(snapshot());
      renderer.markDirty();
    }
  }
}

// ── Context menu ─────────────────────────────────────────
function onContextMenu(e) {
  e.preventDefault();
  const g  = activeGraph();
  const { x, y } = worldMouse(e);
  const hit = renderer.hitNode(x, y, g.nodes);

  if (hit) {
    showCtxMenu(e.clientX, e.clientY, [
      { label: 'Rename…',      action: () => { const l=prompt('Rename:',hit.label||hit.type); if(l!==null){hit.label=l; propsPanel.show(hit); renderer.markDirty();} } },
      { label: 'Duplicate',    action: () => duplicateNodes([hit.id]) },
      { separator: true },
      { label: 'Set as Root',  action: () => { setAsRoot(hit); } },
      { separator: true },
      { label: 'Delete',       danger: true, action: () => deleteSelected([hit.id]) },
    ]);
    if (!g.selectedIds.has(hit.id)) { g.selectedIds = new Set([hit.id]); renderer.markDirty(); }
  } else {
    // Canvas ctx menu — add node sub-menu
    const catOrder = ['composite','action','condition','decorator','attachment'];
    const catLabel = { composite:'Composites', action:'Actions', condition:'Conditions', decorator:'Decorators', attachment:'Attachments' };
    showCtxMenu(e.clientX, e.clientY, [
      { label: 'Add Node ▶', submenu: catOrder.map(cat => ({
          label: catLabel[cat],
          submenu: NODE_TYPES.filter(n => n.category===cat && n.type!=='__Root__').map(n => ({
            label: n.type,
            action: () => createNode(n.type, x - NODE_W/2, y - NODE_H/2)
          }))
        }))
      },
      { separator: true },
      { label: 'Select All',  action: () => { g.selectedIds = new Set(g.nodes.map(n=>n.id)); renderer.markDirty(); } },
      { label: 'Auto Layout', action: () => { autoLayout(activeGraph()); renderer.markDirty(); renderMinimap(); undoStack.push(snapshot()); } },
      { label: 'Fit View',    action: () => renderer.fitAll(g.nodes, document.getElementById('canvas-wrap').clientWidth, document.getElementById('canvas-wrap').clientHeight) },
    ]);
  }
}

function showCtxMenu(clientX, clientY, items) {
  const menu = document.getElementById('ctx-menu');
  menu.innerHTML = '';
  for (const item of items) {
    if (item.separator) { const s = document.createElement('div'); s.className='ctx-separator'; menu.appendChild(s); continue; }
    const el = document.createElement('div');
    el.className = 'ctx-item' + (item.danger ? ' danger' : '');
    el.textContent = item.label;
    if (item.submenu) {
      el.classList.add('ctx-submenu-arrow');
      const sub = document.createElement('div');
      sub.className = 'ctx-sub';
      buildSubMenu(sub, item.submenu);
      el.appendChild(sub);
    } else {
      el.addEventListener('click', () => { hideCtxMenu(); item.action?.(); });
    }
    menu.appendChild(el);
  }
  menu.style.left = clientX + 'px';
  menu.style.top  = clientY + 'px';
  menu.classList.add('visible');
}

function buildSubMenu(container, items) {
  for (const item of items) {
    const el = document.createElement('div');
    el.className = 'ctx-item' + (item.submenu ? ' ctx-submenu-arrow' : '');
    el.textContent = item.label;
    if (item.submenu) {
      const sub = document.createElement('div');
      sub.className = 'ctx-sub';
      buildSubMenu(sub, item.submenu);
      el.appendChild(sub);
    } else {
      el.addEventListener('click', () => { hideCtxMenu(); item.action?.(); });
    }
    container.appendChild(el);
  }
}

function hideCtxMenu() { document.getElementById('ctx-menu').classList.remove('visible'); }
document.addEventListener('click', e => { if (!e.target.closest('#ctx-menu')) hideCtxMenu(); });

// ── Node operations ───────────────────────────────────────
let _nodeIdCounter = 100;

function createNode(type, x, y) {
  const g    = activeGraph();
  const id   = ++_nodeIdCounter;
  const node = {
    id, type,
    label: type,
    props: {},
    extraProps: {},
    x: Math.round(x / 20) * 20,
    y: Math.round(y / 20) * 20,
    w: NODE_W, h: NODE_H,
  };
  // fill defaults
  const schema = getNodeProps(type);
  for (const s of schema) node.props[s.key] = s.default;
  g.nodes.push(node);
  g.selectedIds = new Set([id]);
  propsPanel.show(node);
  undoStack.push(snapshot());
  renderer.markDirty();
  renderMinimap();
  updateStatus();
  return node;
}

function deleteSelected(forceIds) {
  const g   = activeGraph();
  const ids = new Set(forceIds || [...g.selectedIds]);
  g.nodes = g.nodes.filter(n => !ids.has(n.id));
  g.edges = g.edges.filter(e => !ids.has(e.from) && !ids.has(e.to));
  if (g.selectedEdge && (ids.has(g.selectedEdge.from) || ids.has(g.selectedEdge.to))) g.selectedEdge = null;
  g.selectedIds = new Set();
  propsPanel.clear();
  undoStack.push(snapshot());
  renderer.markDirty();
  renderMinimap();
  updateStatus();
}

function deleteSelectedEdge() {
  const g = activeGraph();
  if (!g.selectedEdge) return;
  g.edges = g.edges.filter(e => !(e.from===g.selectedEdge.from && e.to===g.selectedEdge.to));
  g.selectedEdge = null;
  undoStack.push(snapshot());
  renderer.markDirty();
  updateStatus();
}

function duplicateNodes(forceIds) {
  const g    = activeGraph();
  const ids  = forceIds ? new Set(forceIds) : g.selectedIds;
  const oldToNew = {};
  const newNodes = [];
  for (const n of g.nodes.filter(nn => ids.has(nn.id))) {
    const newId = ++_nodeIdCounter;
    oldToNew[n.id] = newId;
    newNodes.push({ ...n, id: newId, x: n.x + 30, y: n.y + 30,
      props: { ...n.props }, extraProps: { ...n.extraProps } });
  }
  g.nodes.push(...newNodes);
  // re-create internal edges
  for (const e of g.edges) {
    if (oldToNew[e.from] && oldToNew[e.to]) g.edges.push({ from: oldToNew[e.from], to: oldToNew[e.to] });
  }
  g.selectedIds = new Set(newNodes.map(n => n.id));
  undoStack.push(snapshot());
  renderer.markDirty();
  updateStatus();
}

function setAsRoot(node) {
  const g = activeGraph();
  // Remove existing Root
  const oldRoot = g.nodes.find(n => n.type === '__Root__');
  if (oldRoot) {
    g.nodes = g.nodes.filter(n => n.id !== oldRoot.id);
    g.edges = g.edges.filter(e => e.from !== oldRoot.id && e.to !== oldRoot.id);
  }
  // Add new Root above this node
  const rootNode = createNode('__Root__', node.x + (NODE_W - NODE_W) / 2, node.y - 110);
  g.edges.push({ from: rootNode.id, to: node.id });
  undoStack.push(snapshot());
  renderer.markDirty();
}

// ── Toolbar ───────────────────────────────────────────────
function bindToolbar() {
  document.getElementById('btn-new-tab').addEventListener('click', () => {
    const name = prompt('Tree name:', 'BT_New');
    if (name) addTab(name);
  });
  document.getElementById('tab-add').addEventListener('click', () => {
    const name = prompt('Tree name:', 'BT_New');
    if (name !== null) addTab(name || 'BT_New');
  });
  document.getElementById('tree-name-input').addEventListener('input', e => {
    const tab = _tabs.find(t => t.id === _activeTab);
    if (tab) {
      tab.name = e.target.value;
      tab.graph.treeName = e.target.value;
      renderTabs();
    }
  });
  document.getElementById('btn-add-root').addEventListener('click', () => {
    const g = activeGraph();
    if (!g.nodes.find(n => n.type === '__Root__')) {
      const w = document.getElementById('canvas-wrap');
      const cx = w.clientWidth / 2, cy = 80;
      const { x, y } = renderer.toWorld(cx, cy);
      createNode('__Root__', x - NODE_W/2, y - NODE_H/2);
    }
  });
  document.getElementById('btn-auto-layout').addEventListener('click', () => {
    autoLayout(activeGraph());
    renderer.fitAll(activeGraph().nodes, document.getElementById('canvas-wrap').clientWidth, document.getElementById('canvas-wrap').clientHeight);
    undoStack.push(snapshot());
    renderMinimap();
  });
  document.getElementById('btn-fit').addEventListener('click', () => {
    const w = document.getElementById('canvas-wrap');
    renderer.fitAll(activeGraph().nodes, w.clientWidth, w.clientHeight);
  });
  document.getElementById('btn-export').addEventListener('click', () => showExportModal());
  document.getElementById('btn-import').addEventListener('click', () => showImportModal());
  document.getElementById('btn-undo').addEventListener('click', undo);
  document.getElementById('btn-redo').addEventListener('click', redo);
  document.getElementById('btn-clear').addEventListener('click', () => {
    if (!confirm('Clear all nodes?')) return;
    const g = activeGraph();
    g.nodes = []; g.edges = []; g.selectedIds = new Set(); g.selectedEdge = null;
    propsPanel.clear(); undoStack.push(snapshot()); renderer.markDirty(); updateStatus();
  });
}

// ── Keyboard ──────────────────────────────────────────────
function bindKeyboard() {
  document.addEventListener('keydown', e => {
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA' || e.target.tagName === 'SELECT') return;
    const mod = e.metaKey || e.ctrlKey;
    const g   = activeGraph();

    if (e.key === ' ') { interact.spaceDown = true; renderer.canvas.style.cursor = 'grab'; }

    if ((e.key === 'Delete' || e.key === 'Backspace') && !mod) {
      e.preventDefault();
      if (g.selectedEdge) deleteSelectedEdge();
      else if (g.selectedIds.size) deleteSelected();
    }
    if (mod && e.key === 'z' && !e.shiftKey) { e.preventDefault(); undo(); }
    if (mod && (e.key === 'y' || (e.key === 'z' && e.shiftKey))) { e.preventDefault(); redo(); }
    if (mod && e.key === 'a') { e.preventDefault(); g.selectedIds = new Set(g.nodes.map(n=>n.id)); renderer.markDirty(); }
    if (mod && e.key === 'd') { e.preventDefault(); duplicateNodes(); }
    if (mod && e.key === 's') { e.preventDefault(); showExportModal(); }
    if (e.key === 'Escape') {
      g.selectedIds = new Set(); g.selectedEdge = null;
      propsPanel.clear(); renderer.markDirty();
    }
    if (e.key === 'f' || e.key === 'F') {
      const w = document.getElementById('canvas-wrap');
      renderer.fitAll(g.nodes, w.clientWidth, w.clientHeight);
    }
  });
  document.addEventListener('keyup', e => {
    if (e.key === ' ') { interact.spaceDown = false; renderer.canvas.style.cursor = 'default'; }
  });
}

// ── Undo/Redo ─────────────────────────────────────────────
function snapshot() {
  const g = activeGraph();
  return JSON.stringify({
    treeName: g.treeName,
    nodes: g.nodes.map(n => ({ ...n, props: {...n.props}, extraProps: {...n.extraProps} })),
    edges: [...g.edges],
    selectedIds: [],
    selectedEdge: null,
  });
}

function restore(json) {
  if (!json) return;
  const data = JSON.parse(json);
  const tab  = _tabs.find(t => t.id === _activeTab);
  if (!tab) return;
  tab.graph.treeName   = data.treeName;
  tab.graph.nodes      = data.nodes;
  tab.graph.edges      = data.edges;
  tab.graph.selectedIds = new Set();
  tab.graph.selectedEdge = null;
  tab.name = data.treeName;
  document.getElementById('tree-name-input').value = data.treeName;
  propsPanel.clear();
  renderTabs();
  renderer.markDirty();
  renderMinimap();
  updateStatus();
}

function undo() { const s = undoStack.undo(); if (s) restore(s); }
function redo() { const s = undoStack.redo(); if (s) restore(s); }

// ── Auto layout ───────────────────────────────────────────
function autoLayout(g) {
  if (!g.nodes.length) return;

  const childMap = {};
  const parentOf = {};
  for (const n of g.nodes) childMap[n.id] = [];
  for (const e of g.edges) {
    const child = g.nodes.find(n => n.id === e.to);
    if (child && !isAttachment(child.type)) {
      childMap[e.from].push(e.to);
      parentOf[e.to] = e.from;
    }
  }
  const nodeById = Object.fromEntries(g.nodes.map(n => [n.id, n]));
  const roots = g.nodes.filter(n => !parentOf[n.id] && !isAttachment(n.type));

  const DX = 200, DY = 130;

  function subW(id) {
    const kids = childMap[id] || [];
    if (!kids.length) return DX;
    return kids.reduce((s,k) => s + subW(k), 0);
  }

  function place(id, cx, y) {
    const n = nodeById[id];
    if (!n) return;
    n.y = y;
    const kids = childMap[id] || [];
    if (!kids.length) { n.x = cx - NODE_W/2; return; }
    let startX = cx - subW(id)/2;
    for (const kid of kids) {
      place(kid, startX + subW(kid)/2, y + DY);
      startX += subW(kid);
    }
    n.x = cx - NODE_W/2;
    // attachments
    const atts = g.edges.filter(e2 => e2.from===id).map(e2 => g.nodes.find(nn=>nn.id===e2.to)).filter(nn=>nn&&isAttachment(nn.type));
    atts.forEach((an,i) => { an.x = n.x + NODE_W + 20; an.y = n.y + i*75; });
  }

  let sx = 400;
  for (const r of roots) {
    place(r.id, sx + subW(r.id)/2, 60);
    sx += subW(r.id) + DX;
  }
}

// ── Minimap ───────────────────────────────────────────────
function buildMinimap() {
  const mm = document.getElementById('minimap');
  mm.addEventListener('click', e => {
    const rect = mm.getBoundingClientRect();
    const g    = activeGraph();
    if (!g.nodes.length) return;
    let minX=Infinity,minY=Infinity,maxX=-Infinity,maxY=-Infinity;
    for (const n of g.nodes) { minX=Math.min(minX,n.x); minY=Math.min(minY,n.y); maxX=Math.max(maxX,n.x+NODE_W); maxY=Math.max(maxY,n.y+NODE_H); }
    const scaleX = 160/(maxX-minX||1), scaleY = 100/(maxY-minY||1), scale=Math.min(scaleX,scaleY)*0.85;
    const mx = (e.clientX - rect.left) / scale + minX;
    const my = (e.clientY - rect.top)  / scale + minY;
    const w  = document.getElementById('canvas-wrap');
    renderer.camera.x = -mx + w.clientWidth/2/renderer.camera.zoom;
    renderer.camera.y = -my + w.clientHeight/2/renderer.camera.zoom;
    renderer.markDirty();
  });
}

function renderMinimap() {
  const mm  = document.getElementById('minimap');
  const mmC = mm.querySelector('canvas');
  if (!mmC) return;
  const ctx = mmC.getContext('2d');
  ctx.clearRect(0,0,160,100);
  const g = activeGraph();
  if (!g.nodes.length) return;

  let minX=Infinity,minY=Infinity,maxX=-Infinity,maxY=-Infinity;
  for (const n of g.nodes) { minX=Math.min(minX,n.x); minY=Math.min(minY,n.y); maxX=Math.max(maxX,n.x+NODE_W); maxY=Math.max(maxY,n.y+NODE_H); }
  const pad=8;
  const scaleX=(160-pad*2)/(maxX-minX||1), scaleY=(100-pad*2)/(maxY-minY||1);
  const scale=Math.min(scaleX,scaleY);

  for (const n of g.nodes) {
    const info = NODE_TYPE_MAP[n.type]||{category:'composite'};
    const col  = getCatColor(info.category);
    ctx.fillStyle = col.header;
    ctx.fillRect(pad+(n.x-minX)*scale, pad+(n.y-minY)*scale, Math.max(4,NODE_W*scale), Math.max(3,NODE_H*scale));
  }
  // viewport rect
  const wrap = document.getElementById('canvas-wrap');
  const wW = wrap.clientWidth, wH = wrap.clientHeight;
  const vpX = -renderer.camera.x;
  const vpY = -renderer.camera.y;
  const vpW = wW / renderer.camera.zoom;
  const vpH = wH / renderer.camera.zoom;
  ctx.strokeStyle = 'rgba(255,255,255,0.4)'; ctx.lineWidth = 1;
  ctx.strokeRect(pad+(vpX-minX)*scale, pad+(vpY-minY)*scale, vpW*scale, vpH*scale);
}

// ── Status bar ────────────────────────────────────────────
function updateStatus() {
  const g = activeGraph();
  document.getElementById('sb-nodes').textContent = `Nodes: ${g.nodes.length}`;
  document.getElementById('sb-edges').textContent = `Edges: ${g.edges.length}`;
  document.getElementById('sb-zoom').textContent  = `Zoom: ${Math.round(renderer?.camera.zoom*100||100)}%`;
}

// ── Export modal ──────────────────────────────────────────
function showExportModal() {
  const xml = exportToXML(activeGraph());
  document.getElementById('modal-xml').value = xml;
  document.getElementById('modal-overlay').classList.add('visible');
  document.getElementById('modal-title').textContent = 'Export XML';
  document.getElementById('modal-xml').readOnly = false;
}

function showImportModal() {
  document.getElementById('modal-xml').value = '';
  document.getElementById('modal-overlay').classList.add('visible');
  document.getElementById('modal-title').textContent = 'Import XML — paste or load XML below';
  document.getElementById('modal-xml').readOnly = false;
}

// ── Utility ───────────────────────────────────────────────
function escHtml(s) { return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
