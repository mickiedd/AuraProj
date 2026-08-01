// app.js — Main application entry point

// ── State ────────────────────────────────────────────────
let _nextId  = 1;
let graph    = null;

function newGraph(name) {
  return {
    treeName: name || 'Ability_Unnamed',
    nodes: [], edges: [],
    selectedIds: new Set(), selectedEdge: null, dragEdge: null,
    sourcePath: '', abilityMeta: {}
  };
}

function activeGraph() { return graph; }

// ── Renderer + subsystems ────────────────────────────────
let renderer, propsPanel, undoStack;

// ── Interaction state ────────────────────────────────────
const interact = {
  isPanning:   false,
  panStart:    { mx: 0, my: 0, cx: 0, cy: 0 },
  isDraggingNodes: false,
  dragNodeStart: null,
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

  graph = newGraph('Ability_Unnamed');
  document.getElementById('tree-name-input').value = graph.treeName;

  const ro = new ResizeObserver(() => {
    renderer.resize(wrap.clientWidth, wrap.clientHeight);
    renderMinimap();
  });
  ro.observe(wrap);
  renderer.resize(wrap.clientWidth, wrap.clientHeight);

  undoStack.push(snapshot());

  buildPalette();
  bindEvents(canvas, wrap);
  bindToolbar();
  bindKeyboard();
  buildMinimap();
  updateStatus();

  initAbilityInfoTab();
  initRoleConfigTab();
  initGEConfigTab();
  bindTabs();
  updateTabTooltips();
});

// ── Tabs ─────────────────────────────────────────────────
function bindTabs() {
  document.querySelectorAll('.tab').forEach(tab => {
    tab.addEventListener('click', () => {
      const target = tab.dataset.tab;
      document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
      document.querySelectorAll('.tab-content').forEach(t => t.classList.remove('active'));
      tab.classList.add('active');
      const content = document.getElementById('tab-content-' + target);
      if (content) content.classList.add('active');
      if (target === 'graph') {
        setTimeout(() => {
          const wrap = document.getElementById('canvas-wrap');
          renderer.resize(wrap.clientWidth, wrap.clientHeight);
          renderMinimap();
        }, 50);
      }
    });
  });
}

// Reflect each tab's backing source-file fullpath into its hover tooltip.
// Called after load/save/refresh so the tooltip always matches current state.
let abilityInfoPath = '';
let roleConfigPath   = '';
let geConfigPath     = '';
function updateTabTooltips() {
  const g = activeGraph();
  const set = (tab, path) => {
    const el = document.querySelector(`.tab[data-tab="${tab}"]`);
    if (el) el.title = path ? `Source: ${path}` : 'No source file loaded';
  };
  set('graph', g ? (g.sourcePath || '') : '');
  set('ability-info', abilityInfoPath);
  set('role-config', roleConfigPath);
  set('ge-config', geConfigPath);
}

// ── Palette ──────────────────────────────────────────────
function buildPalette() {
  const list   = document.getElementById('palette-list');
  const search = document.getElementById('palette-search');
  const groups = {};
  for (const n of NODE_TYPES) {
    if (!groups[n.category]) groups[n.category] = [];
    groups[n.category].push(n);
  }
  const catOrder = ['composite','action'];
  const catLabel = { composite:'Composites', action:'Actions' };

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
    interact.isPanning = true;
    interact.panStart  = { mx: e.clientX, my: e.clientY, cx: renderer.camera.x, cy: renderer.camera.y };
    return;
  }

  if (e.button === 0) {
    const portNode = renderer.hitOutputPort(x, y, g.nodes);
    if (portNode) {
      interact.isDraggingEdge = true;
      interact.dragEdgeFromId = portNode.id;
      g.dragEdge = { fromId: portNode.id, x, y };
      renderer.markDirty();
      return;
    }

    const hit = renderer.hitNode(x, y, g.nodes);
    if (hit) {
      if (!e.shiftKey && !g.selectedIds.has(hit.id)) {
        g.selectedIds = new Set([hit.id]);
        g.selectedEdge = null;
        propsPanel.show(hit, g.abilityMeta);
      } else if (e.shiftKey) {
        if (g.selectedIds.has(hit.id)) g.selectedIds.delete(hit.id);
        else g.selectedIds.add(hit.id);
        propsPanel.show(g.selectedIds.size === 1 ? g.nodes.find(n => g.selectedIds.has(n.id)) : null, g.abilityMeta);
      }
      interact.isDraggingNodes = true;
      interact.dragNodeStart   = [...g.selectedIds].map(id => {
        const n = g.nodes.find(nn => nn.id === id);
        return { id, ox: n.x - x, oy: n.y - y };
      });
      renderer.markDirty();
      return;
    }

    const edge = renderer.hitEdge(x, y, g.edges, g.nodes);
    if (edge) {
      g.selectedIds  = new Set();
      g.selectedEdge = edge;
      propsPanel.clear();
      renderer.markDirty();
      return;
    }

    g.selectedIds  = new Set();
    g.selectedEdge = null;
    propsPanel.show(null, g.abilityMeta);
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
    const sMin = renderer.toScreen(minX, minY);
    const sMax = renderer.toScreen(maxX, maxY);
    const box  = document.getElementById('sel-box');
    box.style.display = 'block';
    box.style.left   = sMin.x + 'px'; box.style.top    = sMin.y + 'px';
    box.style.width  = (sMax.x - sMin.x) + 'px'; box.style.height = (sMax.y - sMin.y) + 'px';

    g.selectedIds = new Set(g.nodes
      .filter(n => n.x+NODE_W > minX && n.x < maxX && n.y+NODE_H > minY && n.y < maxY)
      .map(n => n.id));
    renderer.markDirty();
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
  }
}

function onDblClick(e) {
  const g  = activeGraph();
  const { x, y } = worldMouse(e);
  const hit = renderer.hitNode(x, y, g.nodes);
  if (hit) {
    const label = prompt('Rename node:', hit.label || hit.type);
    if (label !== null) {
      hit.label = label;
      propsPanel.show(hit, g.abilityMeta);
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
      { label: 'Rename…',      action: () => { const l=prompt('Rename:',hit.label||hit.type); if(l!==null){hit.label=l; propsPanel.show(hit,g.abilityMeta); renderer.markDirty();} } },
      { label: 'Duplicate',    action: () => duplicateNodes([hit.id]) },
      { separator: true },
      { label: 'Delete',       danger: true, action: () => deleteSelected([hit.id]) },
    ]);
    if (!g.selectedIds.has(hit.id)) { g.selectedIds = new Set([hit.id]); renderer.markDirty(); }
  } else {
    const catOrder = ['composite','action'];
    const catLabel = { composite:'Composites', action:'Actions' };
    showCtxMenu(e.clientX, e.clientY, [
      { label: 'Add Node ▶', submenu: catOrder.map(cat => ({
          label: catLabel[cat],
          submenu: NODE_TYPES.filter(n => n.category===cat).map(n => ({
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
  const schema = getNodeProps(type);
  for (const s of schema) node.props[s.key] = s.default;
  g.nodes.push(node);
  g.selectedIds = new Set([id]);
  propsPanel.show(node, g.abilityMeta);
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
  propsPanel.show(null, g.abilityMeta);
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
  for (const e of g.edges) {
    if (oldToNew[e.from] && oldToNew[e.to]) g.edges.push({ from: oldToNew[e.from], to: oldToNew[e.to] });
  }
  g.selectedIds = new Set(newNodes.map(n => n.id));
  undoStack.push(snapshot());
  renderer.markDirty();
  updateStatus();
}

// ── Toolbar ───────────────────────────────────────────────
function bindToolbar() {
  document.getElementById('tree-name-input').addEventListener('input', e => {
    graph.treeName = e.target.value;
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
  document.getElementById('btn-export').addEventListener('click', () => saveTree());
  document.getElementById('btn-undo').addEventListener('click', undo);
  document.getElementById('btn-redo').addEventListener('click', redo);
  document.getElementById('btn-clear').addEventListener('click', () => {
    if (!confirm('Clear all nodes?')) return;
    const g = activeGraph();
    g.nodes = []; g.edges = []; g.selectedIds = new Set(); g.selectedEdge = null;
    g.abilityMeta = {};
    propsPanel.show(null, g.abilityMeta); undoStack.push(snapshot()); renderer.markDirty(); updateStatus();
  });
  document.getElementById('btn-open-project').addEventListener('click', openProjectFiles);
  document.getElementById('project-files-close').addEventListener('click', () => {
    document.getElementById('project-files-overlay').style.display = 'none';
  });
  document.getElementById('project-files-overlay').addEventListener('click', e => {
    if (e.target === document.getElementById('project-files-overlay')) document.getElementById('project-files-overlay').style.display = 'none';
  });
}

async function openProjectFiles() {
  const overlay = document.getElementById('project-files-overlay');
  const loadingDiv = document.getElementById('project-loading');
  const progressBar = document.getElementById('project-progress-bar');
  const list = document.getElementById('project-files-list');

  overlay.style.display = '';
  loadingDiv.style.display = '';
  progressBar.style.width = '30%';

  try {
    const resp = await fetch('/list-xml');
    progressBar.style.width = '70%';
    const data = await resp.json();
    progressBar.style.width = '100%';
    const files = data.files || [];
    list.innerHTML = '';
    files.forEach(f => {
      const item = document.createElement('div');
      item.style.cssText = 'padding:8px;cursor:pointer;border-bottom:1px solid var(--border);font-size:12px;';
      item.textContent = f.name;
      item.title = f.path;
      item.addEventListener('click', () => loadFile(f.path));
      item.addEventListener('mouseenter', () => item.style.background = 'var(--hover)');
      item.addEventListener('mouseleave', () => item.style.background = '');
      list.appendChild(item);
    });
    setTimeout(() => { loadingDiv.style.display = 'none'; }, 300);
  } catch (e) {
    loadingDiv.style.display = 'none';
    alert('Backend not available. Please start the server.');
  }
}

async function loadFile(path) {
  try {
    const resp = await fetch('/load', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ path })
    });
    const data = await resp.json();
    const g = activeGraph();
    importFromXML(data.content, g);
    g.sourcePath = data.path;
    document.getElementById('tree-name-input').value = g.treeName;
    propsPanel.show(null, g.abilityMeta);
    renderer.fitAll(g.nodes, document.getElementById('canvas-wrap').clientWidth, document.getElementById('canvas-wrap').clientHeight);
    undoStack.push(snapshot());
    updateStatus();
    updateTabTooltips();
    document.getElementById('project-files-overlay').style.display = 'none';
  } catch (e) {
    console.error('Failed to load file:', e);
    alert('Failed to load file');
  }
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
      if (g.selectedIds.size) deleteSelected();
    }
    if (mod && e.key === 'z' && !e.shiftKey) { e.preventDefault(); undo(); }
    if (mod && (e.key === 'y' || (e.key === 'z' && e.shiftKey))) { e.preventDefault(); redo(); }
    if (mod && e.key === 'a') { e.preventDefault(); g.selectedIds = new Set(g.nodes.map(n=>n.id)); renderer.markDirty(); }
    if (mod && e.key === 'd') { e.preventDefault(); duplicateNodes(); }
    if (mod && e.key === 's') { e.preventDefault(); saveTree(); }
    if (e.key === 'Escape') {
      g.selectedIds = new Set(); g.selectedEdge = null;
      propsPanel.show(null, g.abilityMeta); renderer.markDirty();
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
    sourcePath: g.sourcePath || '',
    abilityMeta: { ...g.abilityMeta },
  });
}

function restore(json) {
  if (!json) return;
  const data = JSON.parse(json);
  if (!graph) return;
  graph.treeName      = data.treeName;
  graph.nodes         = data.nodes;
  graph.edges         = data.edges;
  graph.selectedIds   = new Set();
  graph.selectedEdge  = null;
  graph.sourcePath    = data.sourcePath || '';
  graph.abilityMeta   = data.abilityMeta || {};
  document.getElementById('tree-name-input').value = data.treeName;
  propsPanel.show(null, graph.abilityMeta);
  renderer.markDirty();
  renderMinimap();
  updateStatus();
  updateTabTooltips();
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
    if (child) {
      childMap[e.from].push(e.to);
      parentOf[e.to] = e.from;
    }
  }
  const nodeById = Object.fromEntries(g.nodes.map(n => [n.id, n]));
  const roots = g.nodes.filter(n => !parentOf[n.id]);

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

let _statusFlashTimer = 0;
function setStatusFlash(msg) {
  const bar = document.getElementById('statusbar');
  if (!bar) { console.log('[AuraAbilityGraphEditor]', msg); return; }
  let flash = document.getElementById('sb-flash');
  if (!flash) {
    flash = document.createElement('span');
    flash.id = 'sb-flash';
    flash.style.cssText = 'margin-left:auto;opacity:.9;color:var(--primary);';
    bar.appendChild(flash);
  }
  flash.textContent = msg;
  if (_statusFlashTimer) clearTimeout(_statusFlashTimer);
  _statusFlashTimer = setTimeout(() => { flash.textContent = ''; _statusFlashTimer = 0; }, 4000);
}

// ── Save ─────────────────────────────────────────────────
async function saveTree() {
  const g = activeGraph();
  const xml = exportToXML(g);
  const filename = (g.treeName || 'Ability_Unnamed').replace(/[^\w\-. ]/g, '_') + '.xml';
  setStatusFlash('Saving…');
  try {
    const resp = await fetch('/save', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ filename, xml, source_path: g.sourcePath || '' }),
    });
    const data = await resp.json().catch(() => ({}));
    if (resp.ok && data.ok) {
      if (data.path) g.sourcePath = data.path;
      updateTabTooltips();
      setStatusFlash('Saved → ' + (data.path || filename));
    } else {
      setStatusFlash('Save failed');
      alert('Save failed: ' + (data.error || resp.statusText || 'unknown error'));
    }
  } catch (e) {
    setStatusFlash('Save failed');
    alert('Save failed — backend not reachable. Start the launcher/server.\n' + e);
  }
}

  // ── Ability Info Tab ──────────────────────────────────────
  let abilityInfoData = { abilities: [] };

  async function initAbilityInfoTab() {
    const list = document.getElementById('ability-info-list');
    if (!list) return;

    const refreshBtn = document.getElementById('btn-refresh-abilities');
    if (refreshBtn) {
      refreshBtn.addEventListener('click', async () => {
        await loadAbilityInfoTable();
      });
    }

    const saveBtn = document.getElementById('btn-save-ability-info');
    if (saveBtn) {
      saveBtn.addEventListener('click', async () => {
        await saveAbilityInfoTable();
      });
    }

    await loadAbilityInfoTable();
  }

  async function loadAbilityInfoTable() {
    const list = document.getElementById('ability-info-list');
    if (!list) return;
    list.innerHTML = '<tr><td colspan="5" style="text-align:center;color:var(--text-muted);padding:20px;">Loading...</td></tr>';
    try {
      const resp = await fetch('/list-abilities');
      const data = await resp.json();
      const abilities = data.abilities || [];
      list.innerHTML = '';
      abilities.forEach((ab) => {
        const tr = document.createElement('tr');
        tr.innerHTML = `
          <td>${escHtml(ab.name)}</td>
          <td>${escHtml(ab.tag)}</td>
          <td><input type="text" class="ability-icon" value="" placeholder="icon path" data-path="${ab.path}"/></td>
          <td><input type="text" class="ability-bg" value="" placeholder="background material" data-path="${ab.path}"/></td>
          <td><input type="text" class="ability-lvl" value="1" style="width:60px;" data-path="${ab.path}"/></td>
        `;
        list.appendChild(tr);
      });

      const infoResp = await fetch('/load-ability-info', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{}' });
      const infoData = await infoResp.json();
      if (infoData.ok && infoData.content) {
        abilityInfoPath = infoData.path || abilityInfoPath;
        updateTabTooltips();
        const info = JSON.parse(infoData.content);
        const byTag = Object.fromEntries((info.abilities || []).map(a => [a.abilityTag, a]));
        for (const tr of list.querySelectorAll('tr')) {
          const tagCell = tr.children[1]?.textContent.trim() || '';
          const infoEntry = byTag[tagCell];
          if (infoEntry) {
            tr.querySelector('.ability-icon')?.setAttribute('value', infoEntry.icon || '');
            tr.querySelector('.ability-bg')?.setAttribute('value', infoEntry.backgroundMaterial || '');
            tr.querySelector('.ability-lvl')?.setAttribute('value', String(infoEntry.levelRequirement ?? 1));
          }
        }
      }
    } catch (e) {
      list.innerHTML = '<tr><td colspan="5" style="text-align:center;color:var(--danger);">Failed to load abilities</td></tr>';
    }
  }

  async function saveAbilityInfoTable() {
    const list = document.getElementById('ability-info-list');
    if (!list) return;
    try {
      const resp = await fetch('/list-abilities');
      const data = await resp.json();
      const abilities = data.abilities || [];
      const infoResp = await fetch('/load-ability-info', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{}' });
      const infoData = await infoResp.json();
      let existing = [];
      if (infoData.ok && infoData.content) {
        existing = JSON.parse(infoData.content).abilities || [];
      }
      const byTag = Object.fromEntries(existing.map(a => [a.abilityTag, a]));
      const rows = list.querySelectorAll('tr');
      for (const tr of rows) {
        const tag = (tr.children[1]?.textContent || '').trim();
        const icon = (tr.querySelector('.ability-icon')?.getAttribute('value') || '').trim();
        const bg = (tr.querySelector('.ability-bg')?.getAttribute('value') || '').trim();
        const lvl = parseInt((tr.querySelector('.ability-lvl')?.getAttribute('value') || '1').trim(), 10) || 1;
        if (tag) {
          byTag[tag] = { abilityTag: tag, icon, backgroundMaterial: bg, levelRequirement: lvl };
        }
      }
      const output = { abilities: Object.values(byTag) };
      const saveResp = await fetch('/save-ability-info', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ content: JSON.stringify(output, null, 2) }),
      });
      const saveData = await saveResp.json();
      if (saveData.ok) {
        setStatusFlash('AbilityInfo.json saved');
      } else {
        setStatusFlash('Failed to save AbilityInfo.json');
      }
    } catch (e) {
      setStatusFlash('Failed to save AbilityInfo.json');
    }
  }

// ── Role Config Tab ───────────────────────────────────────
let roleConfigData = { roles: [] };

  async function initRoleConfigTab() {
    const container = document.getElementById('role-config-list');
    if (!container) return;

    const refreshBtn = document.getElementById('btn-refresh-roles');
    if (refreshBtn) {
      refreshBtn.addEventListener('click', async () => {
        await loadRoleConfigTable();
      });
    }

    const saveBtn = document.getElementById('btn-save-role-config');
    if (saveBtn) {
      saveBtn.addEventListener('click', async () => {
        await saveRoleConfig(container);
      });
    }

    await loadRoleConfigTable();
  }

  async function loadRoleConfigTable() {
    const container = document.getElementById('role-config-list');
    if (!container) return;
    container.innerHTML = '<div style="color:var(--text-muted);padding:20px;">Loading...</div>';
    try {
      const resp = await fetch('/load-role-config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{}' });
      const data = await resp.json();
      roleConfigPath = data.path || roleConfigPath;
      updateTabTooltips();
      const config = JSON.parse(data.content);
      roleConfigData = config;
      renderRoleConfig(container, config);
    } catch (e) {
      container.innerHTML = '<div style="color:var(--danger);padding:20px;">Failed to load role config</div>';
    }
  }

  function renderRoleConfig(container, config) {
    container.innerHTML = '';
    const roles = config.roles || [];
    roles.forEach((role, idx) => {
      const card = document.createElement('div');
      card.className = 'role-card';
      const startupAbilities = (role.startupAbilityDefinitions || []).join(', ');
      const lmbAbility = role.lmbAbilityDefinition || '';
      const attrs = JSON.stringify(role.attributes || {});
      card.innerHTML = `
        <div class="role-name">${escHtml(role.role || role.displayName || 'Role ' + (idx+1))}</div>
        <div class="role-row"><label>Role Key</label><input type="text" class="role-key" value="${escHtml(role.role || '')}"/></div>
        <div class="role-row"><label>Display Name</label><input type="text" class="role-display" value="${escHtml(role.displayName || '')}"/></div>
        <div class="role-row"><label>Startup Abilities</label><input type="text" class="role-startup" value="${escHtml(startupAbilities)}" placeholder="comma-separated ability names"/></div>
        <div class="role-row"><label>LMB Ability Definition</label><input type="text" class="role-lmb" value="${escHtml(lmbAbility)}" placeholder="ability definition name"/></div>
        <details style="margin-top:8px;">
          <summary style="cursor:pointer;color:var(--text-muted);font-size:11px;">Raw JSON (view-only)</summary>
          <pre style="background:var(--bg-base);padding:8px;border-radius:4px;font-size:11px;overflow:auto;margin-top:6px;white-space:pre-wrap;word-break:break-all;">${escHtml(JSON.stringify(role, null, 2))}</pre>
        </details>
      `;
      container.appendChild(card);
    });
  }

  async function saveRoleConfig(container) {
    try {
      const original = roleConfigData;
      const roles = (original.roles || []).map((role, idx) => {
        const card = container.querySelectorAll('.role-card')[idx];
        if (!card) return role;
        return {
          ...role,
          role: (card.querySelector('.role-key')?.getAttribute('value') || role.role || '').trim(),
          displayName: (card.querySelector('.role-display')?.getAttribute('value') || role.displayName || '').trim(),
          startupAbilityDefinitions: (card.querySelector('.role-startup')?.getAttribute('value') || '').split(',').map(s => s.trim()).filter(Boolean),
          startupPassiveAbilityDefinitions: role.startupPassiveAbilityDefinitions || [],
          lmbAbilityDefinition: (card.querySelector('.role-lmb')?.getAttribute('value') || role.lmbAbilityDefinition || '').trim(),
          lmbAbility: role.lmbAbility || '',
          mesh: role.mesh || '',
          animBlueprint: role.animBlueprint || '',
          weaponMesh: role.weaponMesh || '',
          weaponSocket: role.weaponSocket || '',
          attributes: role.attributes || {},
        };
      });
      const output = { ...original, roles };
      const resp = await fetch('/save-role-config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ content: JSON.stringify(output, null, 2) }),
      });
      const data = await resp.json();
      if (data.ok) {
        setStatusFlash('RoleConfig.json saved');
        roleConfigData = output;
      } else {
        setStatusFlash('Failed to save RoleConfig.json');
      }
    } catch (e) {
      setStatusFlash('Failed to save RoleConfig.json');
    }
  }

// ── GE Config Tab ────────────────────────────────────────
let geConfigData = null;

  async function initGEConfigTab() {
    const container = document.getElementById('ge-config-list');
    if (!container) return;

    const refreshBtn = document.getElementById('btn-refresh-ge');
    if (refreshBtn) {
      refreshBtn.addEventListener('click', async () => {
        await loadGEConfigTable();
      });
    }

    const saveBtn = document.getElementById('btn-save-ge-config');
    if (saveBtn) {
      saveBtn.addEventListener('click', async () => {
        await saveGEConfigTable(container);
      });
    }

    await loadGEConfigTable();
  }

  async function loadGEConfigTable() {
    const container = document.getElementById('ge-config-list');
    if (!container) return;
    container.innerHTML = '<div style="color:var(--text-muted);padding:20px;">Loading...</div>';
    try {
      const { config, path } = await AbilityData.loadGEConfig();
      geConfigPath = path || geConfigPath;
      updateTabTooltips();
      if (!config) {
        container.innerHTML = '<div style="color:var(--danger);padding:20px;">GameplayEffects.json not found. Click Save to create it.</div>';
        geConfigData = { secondaryAttributes: {}, resistances: {}, pickupEffects: {} };
        return;
      }
      geConfigData = config;
      renderGEConfig(container, config);
    } catch (e) {
      container.innerHTML = '<div style="color:var(--danger);padding:20px;">Failed to load GameplayEffects.json</div>';
    }
  }

  // All secondary/resistance attribute fields with labels
  const GE_SECONDARY_FIELDS = [
    ['Armor', 'Armor'], ['ArmorPenetration', 'Armor Penetration'], ['BlockChance', 'Block Chance'],
    ['CriticalHitChance', 'Crit Chance'], ['CriticalHitDamage', 'Crit Damage'],
    ['CriticalHitResistance', 'Crit Resistance'], ['HealthRegeneration', 'Health Regen'],
    ['ManaRegeneration', 'Mana Regen'], ['MaxHealth', 'Max Health'], ['MaxMana', 'Max Mana'],
  ];
  const GE_RESISTANCE_FIELDS = [
    ['Fire', 'Fire'], ['Lightning', 'Lightning'], ['Arcane', 'Arcane'], ['Physical', 'Physical'],
  ];

  function renderGEConfig(container, config) {
    container.innerHTML = '';
    const sec = config.secondaryAttributes || {};
    const res = config.resistances || {};
    const pickups = config.pickupEffects || {};

    // ── Secondary / Vital Attributes section ──────────────
    const secCard = document.createElement('div');
    secCard.className = 'role-card';
    secCard.innerHTML = '<div class="role-name">Secondary & Vital Attributes</div>';
    GE_SECONDARY_FIELDS.forEach(([key, label]) => {
      const row = document.createElement('div');
      row.className = 'role-row';
      row.innerHTML = `<label>${escHtml(label)}</label><input type="number" step="any" class="ge-sec-${key}" value="${sec[key] ?? 0}"/>`;
      secCard.appendChild(row);
    });
    container.appendChild(secCard);

    // ── Resistance section ────────────────────────────────
    const resCard = document.createElement('div');
    resCard.className = 'role-card';
    resCard.innerHTML = '<div class="role-name">Resistances</div>';
    GE_RESISTANCE_FIELDS.forEach(([key, label]) => {
      const row = document.createElement('div');
      row.className = 'role-row';
      row.innerHTML = `<label>${escHtml(label)}</label><input type="number" step="any" class="ge-res-${key}" value="${res[key] ?? 0}"/>`;
      resCard.appendChild(row);
    });
    container.appendChild(resCard);

    // ── Pickup Effects section ────────────────────────────
    const pickupCard = document.createElement('div');
    pickupCard.className = 'role-card';
    pickupCard.innerHTML = '<div class="role-name">Pickup Effects</div>' +
      '<div style="color:var(--text-muted);font-size:11px;margin-bottom:8px;">Named effects used by AuraEffectActor via InstantEffectName / DurationEffectName / InfiniteEffectName</div>';
    Object.keys(pickups).forEach(name => {
      const eff = pickups[name] || {};
      const row = document.createElement('div');
      row.className = 'role-row';
      row.style.flexDirection = 'column';
      row.style.gap = '4px';
      row.innerHTML = `
        <div style="display:flex;align-items:center;gap:8px;width:100%;">
          <label style="min-width:120px;">${escHtml(name)}</label>
          <input type="text" class="ge-pickup-name" value="${escHtml(name)}" style="flex:0 0 150px;" data-original="${escHtml(name)}"/>
          <button class="btn danger ge-pickup-delete" style="padding:2px 8px;font-size:11px;">Delete</button>
        </div>
        <div style="display:flex;gap:12px;width:100%;">
          <label style="min-width:80px;">Health</label>
          <input type="number" step="any" class="ge-pickup-health" value="${eff.health ?? 0}" style="flex:1;"/>
          <label style="min-width:80px;">Mana</label>
          <input type="number" step="any" class="ge-pickup-mana" value="${eff.mana ?? 0}" style="flex:1;"/>
          <label style="min-width:60px;">Policy</label>
          <select class="ge-pickup-duration" style="flex:1;">
            ${['instant', 'duration', 'infinite'].map(value => `<option value="${value}" ${value === (eff.duration || 'instant') ? 'selected' : ''}>${value}</option>`).join('')}
          </select>
          <label style="min-width:60px;">Seconds</label>
          <input type="number" min="0" step="any" class="ge-pickup-duration-value" value="${eff.durationValue ?? 0}" style="flex:1;"/>
          <label style="min-width:45px;">Period</label>
          <input type="number" min="0" step="any" class="ge-pickup-period" value="${eff.period ?? 0}" style="flex:1;"/>
        </div>
        <div style="display:flex;gap:12px;width:100%;align-items:center;">
          <label style="min-width:145px;">Execute on application</label>
          <input type="checkbox" class="ge-pickup-execute" ${eff.executeOnApplication !== false ? 'checked' : ''}/>
          <label style="min-width:70px;">Asset tags</label>
          <input type="text" class="ge-pickup-tags" value="${escHtml((eff.assetTags || []).join(', '))}" placeholder="Message.HealthPotion" style="flex:1;"/>
        </div>
        <div style="display:flex;gap:12px;width:100%;align-items:center;">
          <label style="min-width:145px;">Attributes (tag/value JSON)</label>
          <textarea class="ge-pickup-attributes" rows="2" placeholder='{"Attributes.Primary.Resilience": 15}' style="flex:1;">${escHtml(JSON.stringify(eff.attributes || {}))}</textarea>
        </div>
      `;
      row.querySelector('.ge-pickup-delete').addEventListener('click', () => {
        row.remove();
      });
      pickupCard.appendChild(row);
    });
    // Add new pickup effect button
    const addBtn = document.createElement('button');
    addBtn.className = 'btn';
    addBtn.style.marginTop = '8px';
    addBtn.textContent = '+ Add Pickup Effect';
    addBtn.addEventListener('click', () => {
      const row = document.createElement('div');
      row.className = 'role-row';
      row.style.flexDirection = 'column';
      row.style.gap = '4px';
      row.innerHTML = `
        <div style="display:flex;align-items:center;gap:8px;width:100%;">
          <label style="min-width:120px;">New Effect</label>
          <input type="text" class="ge-pickup-name" value="" placeholder="effectName" style="flex:0 0 150px;" data-original=""/>
          <button class="btn danger ge-pickup-delete" style="padding:2px 8px;font-size:11px;">Delete</button>
        </div>
        <div style="display:flex;gap:12px;width:100%;">
          <label style="min-width:80px;">Health</label>
          <input type="number" step="any" class="ge-pickup-health" value="0" style="flex:1;"/>
          <label style="min-width:80px;">Mana</label>
          <input type="number" step="any" class="ge-pickup-mana" value="0" style="flex:1;"/>
          <label style="min-width:60px;">Policy</label>
          <select class="ge-pickup-duration" style="flex:1;"><option>instant</option><option>duration</option><option>infinite</option></select>
          <label style="min-width:60px;">Seconds</label>
          <input type="number" min="0" step="any" class="ge-pickup-duration-value" value="0" style="flex:1;"/>
          <label style="min-width:45px;">Period</label>
          <input type="number" min="0" step="any" class="ge-pickup-period" value="0" style="flex:1;"/>
        </div>
        <div style="display:flex;gap:12px;width:100%;align-items:center;">
          <label style="min-width:145px;">Execute on application</label>
          <input type="checkbox" class="ge-pickup-execute" checked/>
          <label style="min-width:70px;">Asset tags</label>
          <input type="text" class="ge-pickup-tags" value="" placeholder="Message.HealthPotion" style="flex:1;"/>
        </div>
        <div style="display:flex;gap:12px;width:100%;align-items:center;">
          <label style="min-width:145px;">Attributes (tag/value JSON)</label>
          <textarea class="ge-pickup-attributes" rows="2" placeholder='{"Attributes.Primary.Resilience": 15}' style="flex:1;">{}</textarea>
        </div>
      `;
      row.querySelector('.ge-pickup-delete').addEventListener('click', () => { row.remove(); });
      pickupCard.insertBefore(row, addBtn);
    });
    pickupCard.appendChild(addBtn);
    container.appendChild(pickupCard);
  }

  async function saveGEConfigTable(container) {
    try {
      const config = geConfigData || { secondaryAttributes: {}, resistances: {}, pickupEffects: {} };

      // Collect secondary/vital
      config.secondaryAttributes = {};
      GE_SECONDARY_FIELDS.forEach(([key]) => {
        const el = container.querySelector(`.ge-sec-${key}`);
        config.secondaryAttributes[key] = el ? parseFloat(el.value) || 0 : 0;
      });

      // Collect resistances
      config.resistances = {};
      GE_RESISTANCE_FIELDS.forEach(([key]) => {
        const el = container.querySelector(`.ge-res-${key}`);
        config.resistances[key] = el ? parseFloat(el.value) || 0 : 0;
      });

      // Collect pickup effects
      config.pickupEffects = {};
      const pickupRows = container.querySelectorAll('#ge-config-list .role-card:last-child .role-row');
      pickupRows.forEach(row => {
        const nameEl = row.querySelector('.ge-pickup-name');
        const healthEl = row.querySelector('.ge-pickup-health');
        const manaEl = row.querySelector('.ge-pickup-mana');
        const durEl = row.querySelector('.ge-pickup-duration');
        const durValueEl = row.querySelector('.ge-pickup-duration-value');
        const periodEl = row.querySelector('.ge-pickup-period');
        const executeEl = row.querySelector('.ge-pickup-execute');
        const tagsEl = row.querySelector('.ge-pickup-tags');
        const attributesEl = row.querySelector('.ge-pickup-attributes');
        if (!nameEl) return;
        const name = nameEl.value.trim();
        if (!name) return;
        const duration = durEl ? durEl.value.trim() || 'instant' : 'instant';
        const effect = {
          duration,
          health: healthEl ? parseFloat(healthEl.value) || 0 : 0,
          mana: manaEl ? parseFloat(manaEl.value) || 0 : 0,
        };
        const durationValue = durValueEl ? parseFloat(durValueEl.value) || 0 : 0;
        if (duration === 'duration') effect.durationValue = durationValue;
        const period = periodEl ? parseFloat(periodEl.value) || 0 : 0;
        if (period > 0) {
          effect.period = period;
          effect.executeOnApplication = executeEl ? executeEl.checked : true;
        }
        const assetTags = tagsEl ? tagsEl.value.split(',').map(tag => tag.trim()).filter(Boolean) : [];
        if (assetTags.length) effect.assetTags = assetTags;
        const attributes = attributesEl && attributesEl.value.trim() ? JSON.parse(attributesEl.value) : {};
        if (attributes && typeof attributes === 'object' && !Array.isArray(attributes) && Object.keys(attributes).length) {
          effect.attributes = attributes;
        }
        config.pickupEffects[name] = effect;
      });

      const ok = await AbilityData.saveGEConfig(config);
      if (ok) {
        setStatusFlash('GameplayEffects.json saved');
        geConfigData = config;
      } else {
        setStatusFlash('Failed to save GameplayEffects.json');
      }
    } catch (e) {
      setStatusFlash('Failed to save GameplayEffects.json');
    }
  }

// ── Utility ───────────────────────────────────────────────
function escHtml(s) { return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }
