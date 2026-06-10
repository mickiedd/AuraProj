// graph.js — Canvas rendering, pan/zoom, hit-testing, node/edge drawing

const NODE_W = 180;
const NODE_H = 72;      // default; dynamic per node
const PORT_R = 6;
const HEADER_H = 30;    // taller to fit type + label name
const BODY_LINE_H = 15; // px per info line in body

class GraphRenderer {
  constructor(canvas, getGraph) {
    this.canvas   = canvas;
    this.ctx      = canvas.getContext('2d');
    this.getGraph = getGraph;  // () => current graph object
    this.camera   = { x: 0, y: 0, zoom: 1.0 };
    this._raf     = null;
    this._dirty   = true;
    this._scheduleRender();
  }

  markDirty() { this._dirty = true; }

  _scheduleRender() {
    this._raf = requestAnimationFrame(() => {
      if (this._dirty) { this._render(); this._dirty = false; }
      this._scheduleRender();
    });
  }

  resize(w, h) {
    this.canvas.width  = w * devicePixelRatio;
    this.canvas.height = h * devicePixelRatio;
    this.canvas.style.width  = w + 'px';
    this.canvas.style.height = h + 'px';
    this.ctx.scale(devicePixelRatio, devicePixelRatio);
    this._render();
  }

  // world ↔ screen transforms
  toScreen(wx, wy) {
    return {
      x: (wx + this.camera.x) * this.camera.zoom,
      y: (wy + this.camera.y) * this.camera.zoom
    };
  }
  toWorld(sx, sy) {
    return {
      x: sx / this.camera.zoom - this.camera.x,
      y: sy / this.camera.zoom - this.camera.y
    };
  }

  fitAll(nodes, wrapW, wrapH) {
    if (!nodes.length) return;
    let minX=Infinity,minY=Infinity,maxX=-Infinity,maxY=-Infinity;
    for (const n of nodes) {
      minX = Math.min(minX, n.x); minY = Math.min(minY, n.y);
      maxX = Math.max(maxX, n.x + NODE_W); maxY = Math.max(maxY, n.y + NODE_H);
    }
    const pad = 60;
    const scaleX = (wrapW - pad*2) / (maxX - minX || 1);
    const scaleY = (wrapH - pad*2) / (maxY - minY || 1);
    this.camera.zoom = Math.max(0.2, Math.min(1.5, Math.min(scaleX, scaleY)));
    this.camera.x = -minX + pad / this.camera.zoom;
    this.camera.y = -minY + pad / this.camera.zoom;
    this.markDirty();
  }

  _render() {
    const ctx = this.ctx;
    const W   = this.canvas.width  / devicePixelRatio;
    const H   = this.canvas.height / devicePixelRatio;
    const g   = this.getGraph();

    ctx.save();
    ctx.clearRect(0, 0, W, H);

    // background grid
    this._drawGrid(ctx, W, H);

    ctx.save();
    ctx.scale(this.camera.zoom, this.camera.zoom);
    ctx.translate(this.camera.x, this.camera.y);

    // edges first
    for (const e of g.edges) {
      this._drawEdge(ctx, e, g.nodes, g.selectedEdge);
    }
    // drag-preview edge
    if (g.dragEdge) this._drawDragEdge(ctx, g.dragEdge, g.nodes);

    // nodes
    for (const n of g.nodes) {
      this._drawNode(ctx, n, g.selectedIds);
    }

    ctx.restore();
    ctx.restore();
  }

  _drawGrid(ctx, W, H) {
    const z    = this.camera.zoom;
    const step = 20 * Math.max(1, Math.round(1/z));
    const offX = ((this.camera.x * z) % (step * z) + step * z) % (step * z);
    const offY = ((this.camera.y * z) % (step * z) + step * z) % (step * z);
    ctx.strokeStyle = '#2a2a2a';
    ctx.lineWidth   = 1;
    ctx.beginPath();
    for (let x = offX; x < W; x += step * z) { ctx.moveTo(x, 0); ctx.lineTo(x, H); }
    for (let y = offY; y < H; y += step * z) { ctx.moveTo(0, y); ctx.lineTo(W, y); }
    ctx.stroke();
  }

  _drawNode(ctx, n, selectedIds) {
    const info  = NODE_TYPE_MAP[n.type] || { category: 'composite', label: n.type };
    const col   = getCatColor(info.category);
    const sel   = selectedIds && selectedIds.has(n.id);

    // Build info lines first so we can size the node
    const lines  = this._getInfoLines(n);
    const bodyH  = Math.max(32, lines.length * BODY_LINE_H + 10);
    const w      = n.w || NODE_W;
    const h      = HEADER_H + bodyH;
    // update node height so hit-testing & ports stay accurate
    n.h = h;

    // ── shadow / glow ──
    if (sel) { ctx.shadowColor = col.header; ctx.shadowBlur = 14; }

    // ── body ──
    ctx.fillStyle = col.bg;
    this._roundRect(ctx, n.x, n.y, w, h, 6);
    ctx.fill();

    ctx.shadowBlur = 0; ctx.shadowColor = 'transparent';

    // ── header gradient ──
    const grad = ctx.createLinearGradient(n.x, n.y, n.x, n.y + HEADER_H);
    grad.addColorStop(0, col.header);
    grad.addColorStop(1, this._darken(col.header, 0.22));
    ctx.fillStyle = grad;
    this._roundRect(ctx, n.x, n.y, w, HEADER_H, [6, 6, 0, 0]);
    ctx.fill();

    // ── border ──
    ctx.strokeStyle = sel ? '#ffffff' : col.header;
    ctx.lineWidth   = sel ? 2 : 1;
    this._roundRect(ctx, n.x, n.y, w, h, 6);
    ctx.stroke();

    // ── header: type (small, top) + label (prominent, bottom) ──
    ctx.save();
    if (n.label && n.label !== n.type) {
      // Two-line header: type dim on top, label bright on bottom
      ctx.fillStyle    = 'rgba(255,255,255,0.6)';
      ctx.font         = '9px "Segoe UI", system-ui, sans-serif';
      ctx.textAlign    = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillText(n.type, n.x + w / 2, n.y + 7);

      ctx.fillStyle    = '#ffffff';
      ctx.font         = 'bold 11px "Segoe UI", system-ui, sans-serif';
      ctx.textBaseline = 'middle';
      ctx.fillText(this._truncate(ctx, n.label, w - 10), n.x + w / 2, n.y + HEADER_H - 8);
    } else {
      // Single centered type label
      ctx.fillStyle    = '#ffffff';
      ctx.font         = 'bold 11px "Segoe UI", system-ui, sans-serif';
      ctx.textAlign    = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillText(n.type, n.x + w / 2, n.y + HEADER_H / 2);
    }
    ctx.restore();

    // ── info lines in body ──
    if (lines.length > 0) {
      const startY = n.y + HEADER_H + 5 + BODY_LINE_H / 2;
      for (let i = 0; i < lines.length; i++) {
        const line = lines[i];
        const ly   = startY + i * BODY_LINE_H;
        const pad  = 8;
        const maxW = w - pad * 2;

        ctx.save();
        ctx.textAlign    = 'left';
        ctx.textBaseline = 'middle';

        // key label (dim)
        if (line.key) {
          ctx.fillStyle = 'rgba(180,180,180,0.55)';
          ctx.font      = '9px "Segoe UI", system-ui, sans-serif';
          const keyW = Math.min(ctx.measureText(line.key + ':').width + 2, maxW * 0.45);
          ctx.fillText(this._truncate(ctx, line.key + ':', keyW + 4), n.x + pad, ly);

          // value (bright)
          ctx.fillStyle = line.valueColor || '#e8e8e8';
          ctx.font      = '10px "Consolas", "Segoe UI", monospace';
          const valX = n.x + pad + keyW + 4;
          ctx.fillText(this._truncate(ctx, line.value, maxW - keyW - 4), valX, ly);
        } else {
          // single full-width value (no key prefix)
          ctx.fillStyle = line.valueColor || '#e0e0e0';
          ctx.font      = '10px "Segoe UI", system-ui, sans-serif';
          ctx.textAlign = 'center';
          ctx.fillText(this._truncate(ctx, line.value, maxW), n.x + w / 2, ly);
        }
        ctx.restore();
      }
    } else {
      // empty body hint
      ctx.save();
      ctx.fillStyle    = 'rgba(255,255,255,0.18)';
      ctx.font         = 'italic 10px "Segoe UI", system-ui, sans-serif';
      ctx.textAlign    = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillText('(no properties)', n.x + w / 2, n.y + HEADER_H + bodyH / 2);
      ctx.restore();
    }

    // ── ports ──
    if (!isAttachment(n.type)) {
      this._drawPort(ctx, n.x + w / 2, n.y,     col.port, 'in',  false);
      if (canHaveChildren(n.type))
        this._drawPort(ctx, n.x + w / 2, n.y + h, col.port, 'out', false);
    } else {
      this._drawPort(ctx, n.x, n.y + h / 2, col.port, 'in', false);
    }
  }

  /** Build display lines [{key, value, valueColor}] from node props */
  _getInfoLines(n) {
    const p = n.props || {};
    const lines = [];

    const add = (key, value, valueColor) => {
      const v = String(value ?? '').trim();
      if (v === '' || v === 'undefined') return;
      lines.push({ key, value: v, valueColor });
    };
    const addFull = (value, valueColor) => {
      const v = String(value ?? '').trim();
      if (v === '') return;
      lines.push({ key: null, value: v, valueColor });
    };
    const opSym = op => ({Equal:'==',NotEqual:'!=',Greater:'>',Less:'<',GreaterEqual:'>=',LessEqual:'<=',
                          Assign:'=',Add:'+',Sub:'-',Mul:'\u00d7',Div:'\u00f7'}[op] || op || '?');
    const resultColor = r => r === 'BT_FAILURE' ? '#f44747' : r === 'BT_RUNNING' ? '#ffd580' : '#4ec9b0';

    switch (n.type) {
      // ── Actions ── (plugin reads: "Method", "ResultOption")
      case 'Action':
        add('method', p.Method, '#7ec8e3');
        if (p.ResultOption && p.ResultOption !== 'BT_SUCCESS')
          add('\u2192', p.ResultOption, resultColor(p.ResultOption));
        break;
      case 'Assignment':
        if (p.Opl || p.Opr) addFull(`${p.Opl || '?'}  \u2190  ${p.Opr || '?'}`, '#dcdcaa');
        break;
      case 'Compute':
        if (p.Opl) addFull(`${p.Opl} = ${p.Opr1||'?'} ${opSym(p.Operator)} ${p.Opr2||'?'}`, '#dcdcaa');
        break;
      case 'Wait':
        add('time', (p.Time || '\u2014') + ' s', '#c3e88d'); break;
      case 'WaitFrames':
        add('frames', p.Frames || '\u2014', '#c3e88d'); break;
      case 'WaitforSignal':
        add('signal', p.Signal, '#f78c6c'); break;
      case 'End':
        add('status', p.EndStatus || 'BT_SUCCESS', resultColor(p.EndStatus)); break;

      // ── Conditions ── (plugin reads: "Opl", "Operator", "Opr")
      case 'Condition':
        if (p.Opl || p.Opr)
          addFull(`${p.Opl||'?'}  ${opSym(p.Operator)}  ${p.Opr||'?'}`, '#ffd580');
        break;
      case 'And':   addFull('ALL children true', '#ffd580'); break;
      case 'Or':    addFull('ANY child true',    '#ffd580'); break;
      case 'True':  addFull('\u2713  Always Success', '#4ec9b0'); break;
      case 'False': addFull('\u2717  Always Failure', '#f44747'); break;
      case 'ConditionBase': addFull('(custom condition)', 'rgba(255,255,255,0.35)'); break;

      // ── Decorators ── (plugin class names have "Decorator" prefix)
      case 'DecoratorLoop':
        add('count', (p.Count === '-1' || p.Count === -1) ? '\u221e' : (p.Count || '\u221e'), '#c3e88d'); break;
      case 'DecoratorLoopUntil':
        add('count', p.Count || '1', '#c3e88d');
        add('until', p.Until || 'true', '#ffd580'); break;
      case 'DecoratorRepeat': case 'DecoratorCount':
        add('count', p.Count || '1', '#c3e88d'); break;
      case 'DecoratorCountLimit':
        add('max', p.Count || '1', '#c3e88d'); break;
      case 'DecoratorTime':   add('time', (p.Time || '\u2014') + ' s', '#c3e88d'); break;
      case 'DecoratorFrames': add('frames', p.Frames || '\u2014', '#c3e88d'); break;
      case 'DecoratorFailureUntil': add('fail \u00d7', p.Count || '1', '#f44747'); break;
      case 'DecoratorSuccessUntil': add('ok \u00d7', p.Count || '1', '#4ec9b0'); break;
      case 'DecoratorIterator':
        add('list', p.Opl, '#7ec8e3');
        add('var',  p.Opr, '#b5cea8'); break;
      case 'DecoratorLog':    add('msg', p.Message, '#b5cea8'); break;
      case 'DecoratorWeight': add('w', p.Weight || '1.0', '#ffd580'); break;
      case 'DecoratorNot':           addFull('\u27f5 inverts child',  'rgba(255,255,255,0.4)'); break;
      case 'DecoratorAlwaysSuccess': addFull('\u2192 always \u2713', '#4ec9b0'); break;
      case 'DecoratorAlwaysFailure': addFull('\u2192 always \u2717', '#f44747'); break;
      case 'DecoratorAlwaysRunning': addFull('\u2192 always \u27f3', '#ffd580'); break;
      case 'WithPrecondition': addFull('runs if precond passes', 'rgba(255,255,255,0.35)'); break;

      // ── Composites ──
      case 'Selector':          addFull('try each  \u2192  first \u2713', 'rgba(255,255,255,0.35)'); break;
      case 'Sequence':          addFull('run all  \u2192  stop on \u2717', 'rgba(255,255,255,0.35)'); break;
      case 'Parallel': {
        const fp = (p.FailurePolicy || '').replace('FailOnOne_SucceedOnAll','1\u2717 stops');
        addFull(fp || 'parallel', 'rgba(255,255,255,0.45)'); break;
      }
      case 'IfElse':            addFull('[cond] \u2192 [then] / [else]', 'rgba(255,255,255,0.35)'); break;
      case 'SelectorLoop':      addFull('loop until one \u2713', 'rgba(255,255,255,0.35)'); break;
      case 'SelectorProbability': addFull('pick by weight', 'rgba(255,255,255,0.35)'); break;
      case 'SelectorStochastic':  addFull('pick randomly',  'rgba(255,255,255,0.35)'); break;
      case 'SequenceStochastic':  addFull('run all (random order)', 'rgba(255,255,255,0.35)'); break;
      case 'ReferencedBehavior':  add('ref', p.ReferencedTreePath, '#7ec8e3'); break;

      // ── Attachments ── (plugin reads: "Opl","Operator","Opr","Phase","Negate")
      case 'Precondition':
        if (p.Opl || p.Opr)
          addFull(`${p.Opl||'?'}  ${opSym(p.Operator)}  ${p.Opr||'?'}`, '#ffd580');
        if (p.Phase) add('phase', p.Phase, '#bb9af7');
        if (p.Negate === true || p.Negate === 'true') addFull('(negated)', '#f44747');
        break;
      case 'Effector':
        if (p.Opl || p.Opr)
          addFull(`${p.Opl||'?'} ${opSym(p.Operator)} ${p.Opr||'?'}`, '#dcdcaa');
        if (p.Phase) add('on', p.Phase, '#bb9af7');
        break;
      case 'Event':
        add('event', p.EventName, '#f78c6c');
        if (p.TriggeredOnce === true || p.TriggeredOnce === 'true')
          addFull('once only', '#ffd580');
        break;

      // ── Root ──
      case '__Root__': addFull('\u25b6  entry point', '#4ec9b0'); break;

      default: {
        const schema = getNodeProps(n.type);
        let shown = 0;
        for (const s of schema) {
          const v = p[s.key];
          if (v !== undefined && v !== '' && shown < 2) { add(s.key, v); shown++; }
        }
        break;
      }
    }

    return lines;
  }

  /** Darken a hex color by `amount` (0-1) */
  _darken(hex, amount) {
    let r = parseInt(hex.slice(1,3),16);
    let g = parseInt(hex.slice(3,5),16);
    let b = parseInt(hex.slice(5,7),16);
    r = Math.round(r * (1 - amount));
    g = Math.round(g * (1 - amount));
    b = Math.round(b * (1 - amount));
    return '#' + [r,g,b].map(v => v.toString(16).padStart(2,'0')).join('');
  }

  _drawPort(ctx, x, y, color, _type, hovered) {
    ctx.beginPath();
    ctx.arc(x, y, PORT_R, 0, Math.PI * 2);
    ctx.fillStyle   = hovered ? '#fff' : '#1e1e1e';
    ctx.strokeStyle = color;
    ctx.lineWidth   = 2;
    ctx.fill();
    ctx.stroke();
  }

  _drawEdge(ctx, e, nodes, selectedEdge) {
    const fromNode = nodes.find(n => n.id === e.from);
    const toNode   = nodes.find(n => n.id === e.to);
    if (!fromNode || !toNode) return;

    const fw = fromNode.w || NODE_W, fh = fromNode.h || NODE_H;
    const tw = toNode.w   || NODE_W, th = toNode.h   || NODE_H;

    let sx, sy, tx, ty;
    if (isAttachment(toNode.type)) {
      sx = fromNode.x + fw;   sy = fromNode.y + fh / 2;
      tx = toNode.x;           ty = toNode.y  + th / 2;
    } else {
      sx = fromNode.x + fw / 2; sy = fromNode.y + fh;
      tx = toNode.x   + tw / 2; ty = toNode.y;
    }

    const sel = selectedEdge && selectedEdge.from === e.from && selectedEdge.to === e.to;
    ctx.save();
    ctx.strokeStyle = sel ? '#fff' : (isAttachment(toNode.type) ? '#e74c3c' : '#666');
    ctx.lineWidth   = sel ? 2.5 : 1.8;
    ctx.setLineDash(isAttachment(toNode.type) ? [5,3] : []);
    ctx.beginPath();
    const cy = (sy + ty) / 2;
    ctx.moveTo(sx, sy);
    ctx.bezierCurveTo(sx, cy, tx, cy, tx, ty);
    ctx.stroke();
    ctx.setLineDash([]);

    // arrowhead at destination
    const angle = isAttachment(toNode.type)
      ? Math.atan2(ty - sy, tx - sx)
      : Math.PI / 2;  // pointing down
    ctx.fillStyle = ctx.strokeStyle;
    ctx.save();
    ctx.translate(tx, ty);
    ctx.rotate(angle);
    ctx.beginPath();
    ctx.moveTo(0, -5); ctx.lineTo(4, 2); ctx.lineTo(-4, 2);
    ctx.closePath(); ctx.fill();
    ctx.restore();
    ctx.restore();
  }

  _drawDragEdge(ctx, de, nodes) {
    const fromNode = nodes.find(n => n.id === de.fromId);
    if (!fromNode) return;
    const sx = fromNode.x + (fromNode.w || NODE_W) / 2;
    const sy = fromNode.y + (fromNode.h || NODE_H);
    const tx = de.x, ty = de.y;
    const cy = (sy + ty) / 2;
    ctx.save();
    ctx.strokeStyle = '#007acc'; ctx.lineWidth = 2; ctx.setLineDash([6,3]);
    ctx.beginPath();
    ctx.moveTo(sx, sy); ctx.bezierCurveTo(sx, cy, tx, cy, tx, ty);
    ctx.stroke(); ctx.restore();
  }

  _roundRect(ctx, x, y, w, h, r) {
    if (typeof r === 'number') r = [r,r,r,r];
    const [tl, tr, br, bl] = r;
    ctx.beginPath();
    ctx.moveTo(x + tl, y);
    ctx.lineTo(x + w - tr, y);         ctx.arcTo(x+w, y,    x+w,   y+h,  tr);
    ctx.lineTo(x+w, y+h-br);           ctx.arcTo(x+w, y+h,  x,     y+h,  br);
    ctx.lineTo(x + bl, y + h);         ctx.arcTo(x,   y+h,  x,     y,    bl);
    ctx.lineTo(x, y + tl);             ctx.arcTo(x,   y,    x+w,   y,    tl);
    ctx.closePath();
  }

  _truncate(ctx, text, maxW) {
    if (ctx.measureText(text).width <= maxW) return text;
    while (text.length > 1 && ctx.measureText(text + '…').width > maxW) text = text.slice(0,-1);
    return text + '…';
  }

  // ── Hit testing ──────────────────────────────────────────
  hitNode(wx, wy, nodes) {
    // iterate in reverse so top-most node wins
    for (let i = nodes.length - 1; i >= 0; i--) {
      const n = nodes[i];
      const w = n.w || NODE_W, h = n.h || NODE_H;
      if (wx >= n.x && wx <= n.x+w && wy >= n.y && wy <= n.y+h) return n;
    }
    return null;
  }

  hitOutputPort(wx, wy, nodes) {
    for (const n of nodes) {
      if (isAttachment(n.type) || !canHaveChildren(n.type)) continue;
      const px = n.x + (n.w || NODE_W) / 2;
      const py = n.y + (n.h || NODE_H);
      if (Math.hypot(wx-px, wy-py) <= PORT_R + 4) return n;
    }
    return null;
  }

  hitInputPort(wx, wy, nodes) {
    for (const n of nodes) {
      if (isAttachment(n.type)) {
        // left port
        const px = n.x, py = n.y + (n.h || NODE_H) / 2;
        if (Math.hypot(wx-px, wy-py) <= PORT_R + 4) return n;
      } else {
        const px = n.x + (n.w || NODE_W) / 2;
        const py = n.y;
        if (Math.hypot(wx-px, wy-py) <= PORT_R + 4) return n;
      }
    }
    return null;
  }

  hitEdge(wx, wy, edges, nodes) {
    for (const e of edges) {
      const fromNode = nodes.find(n => n.id === e.from);
      const toNode   = nodes.find(n => n.id === e.to);
      if (!fromNode || !toNode) continue;
      let sx, sy, tx, ty;
      if (isAttachment(toNode.type)) {
        sx = fromNode.x + (fromNode.w||NODE_W); sy = fromNode.y + (fromNode.h||NODE_H)/2;
        tx = toNode.x;                          ty = toNode.y + (toNode.h||NODE_H)/2;
      } else {
        sx = fromNode.x+(fromNode.w||NODE_W)/2; sy = fromNode.y+(fromNode.h||NODE_H);
        tx = toNode.x+(toNode.w||NODE_W)/2;     ty = toNode.y;
      }
      if (this._pointNearBezier(wx, wy, sx, sy, tx, ty, 8)) return e;
    }
    return null;
  }

  _pointNearBezier(px, py, sx, sy, tx, ty, thresh) {
    const cy = (sy+ty)/2;
    const steps = 16;
    let prev = {x:sx,y:sy};
    for (let i=1;i<=steps;i++) {
      const t = i/steps;
      const mt = 1-t;
      const x = mt*mt*mt*sx + 3*mt*mt*t*sx + 3*mt*t*t*tx + t*t*t*tx;
      const y = mt*mt*mt*sy + 3*mt*mt*t*cy + 3*mt*t*t*cy + t*t*t*ty;
      if (this._pointNearSeg(px,py,prev.x,prev.y,x,y,thresh)) return true;
      prev = {x,y};
    }
    return false;
  }

  _pointNearSeg(px,py,ax,ay,bx,by,thresh) {
    const dx=bx-ax,dy=by-ay,len2=dx*dx+dy*dy;
    if (len2===0) return Math.hypot(px-ax,py-ay)<thresh;
    const t=Math.max(0,Math.min(1,((px-ax)*dx+(py-ay)*dy)/len2));
    return Math.hypot(px-(ax+t*dx), py-(ay+t*dy)) < thresh;
  }
}

function opLabel(op) {
  const m = {Equal:'==',NotEqual:'!=',Greater:'>',Less:'<',GreaterEqual:'>=',LessEqual:'<=',
             Assign:'=',Add:'+',Subtract:'-',Multiply:'*',Divide:'/'};
  return m[op] || op;
}
