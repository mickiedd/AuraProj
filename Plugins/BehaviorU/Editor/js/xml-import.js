// xml-import.js — Parse behaviac XML into graph nodes/edges
// Supports both formats:
//   <property name="Key" value="Val"/>   ← plugin format
//   <property Key="Val"/>                ← legacy attribute-per-key format

function importFromXML(xmlStr, graph) {
  let doc;
  try {
    doc = new DOMParser().parseFromString(xmlStr, 'application/xml');
  } catch(e) { alert('XML parse error: ' + e.message); return; }
  const errNode = doc.querySelector('parsererror');
  if (errNode) { alert('Invalid XML:\n' + errNode.textContent.slice(0,300)); return; }

  const behavior = doc.querySelector('behavior');
  if (!behavior) { alert('No <behavior> element found.'); return; }

  graph.treeName = behavior.getAttribute('name') || 'Imported';
  graph.nodes = [];
  graph.edges = [];

  let idCounter = 1;
  const usedIds = new Set();

  function allocId(attrId) {
    const n = parseInt(attrId, 10);
    if (!isNaN(n) && !usedIds.has(n)) { usedIds.add(n); return n; }
    while (usedIds.has(idCounter)) idCounter++;
    usedIds.add(idCounter);
    return idCounter++;
  }

  /** Read <property> children → {key: value} map, normalizing both wire formats */
  function readProps(el) {
    const props = {};
    for (const propEl of el.children) {
      if (propEl.tagName !== 'property') continue;
      const nameAttr  = propEl.getAttribute('name');
      const valueAttr = propEl.getAttribute('value');
      if (nameAttr !== null) {
        props[normPropKey(nameAttr)] = valueAttr ?? '';
      } else {
        for (let i = 0; i < propEl.attributes.length; i++) {
          const a = propEl.attributes[i];
          props[normPropKey(a.name)] = a.value;
        }
      }
    }
    return props;
  }

  /** Normalize legacy property key names */
  function normPropKey(k) {
    const map = {
      'LeftOperand':    'Opl',
      'RightOperand':   'Opr',
      'MethodName':     'Method',
      'ResultFuncName': 'ResultOption',
    };
    return map[k] || k;
  }

  /**
   * Normalize class names to editor type strings.
   * Handles:
   *  - "behaviac::Selector" → "Selector"  (namespace strip)
   *  - "Root"               → "__Root__"  (plugin has no Root class)
   *  - "WaitForSignal"      → "WaitforSignal" (case fix)
   *  - "AlwaysFailure"      → "DecoratorAlwaysFailure" (prefix fix)
   *  - "ReferenceBehavior"  → "ReferencedBehavior" (spelling fix)
   */
  function normType(rawClass) {
    // Strip namespace prefix
    let t = rawClass.split(':').pop().trim();

    // Canonical remaps
    const remap = {
      // Root wrapper — editor uses __Root__ since plugin has no Root class
      'Root':             '__Root__',
      // Decorator prefix — plugin uses "Decorator" prefix on these
      'AlwaysFailure':    'DecoratorAlwaysFailure',
      'AlwaysRunning':    'DecoratorAlwaysRunning',
      'AlwaysSuccess':    'DecoratorAlwaysSuccess',
      'Not':              'DecoratorNot',
      'Loop':             'DecoratorLoop',
      'LoopUntil':        'DecoratorLoopUntil',
      'Repeat':           'DecoratorRepeat',
      'Count':            'DecoratorCount',
      'CountLimit':       'DecoratorCountLimit',
      'Time':             'DecoratorTime',
      'Frames':           'DecoratorFrames',
      'FailureUntil':     'DecoratorFailureUntil',
      'SuccessUntil':     'DecoratorSuccessUntil',
      'Iterator':         'DecoratorIterator',
      'Log':              'DecoratorLog',
      'Weight':           'DecoratorWeight',
      // Name fixes
      'WaitForSignal':    'WaitforSignal',
      'ReferenceBehavior':'ReferencedBehavior',
    };
    return remap[t] || t;
  }

  function parseNode(el, depth, sibIdx, parentId) {
    const rawId   = el.getAttribute('id');
    const rawType = el.getAttribute('class') || el.tagName;
    const id      = allocId(rawId);
    const type    = normType(rawType);

    const props = readProps(el);
    // Use the "Name" property as display label; remove it from props
    const label = props.Name || getDisplayLabel(type);
    delete props.Name;

    const x = sibIdx * 200 + 60;
    const y = depth  * 130 + 60;

    graph.nodes.push({ id, type, label, props, x, y, w: 180, h: 72, extraProps:{} });
    if (parentId !== null) graph.edges.push({ from: parentId, to: id });

    let childIdx = 0;
    for (const child of el.children) {
      if (child.tagName === 'node') {
        parseNode(child, depth + 1, childIdx++, id);
      } else if (child.tagName === 'attachment') {
        parseAttachment(child, id);
      }
    }
  }

  function parseAttachment(el, parentId) {
    const rawId   = el.getAttribute('id');
    const rawType = el.getAttribute('class') || 'Precondition';
    const id      = allocId(rawId);
    const type    = normType(rawType);
    const props   = readProps(el);
    const label   = props.Name || getDisplayLabel(type);
    delete props.Name;

    const parentNode = graph.nodes.find(n => n.id === parentId);
    const x = (parentNode?.x || 0) + 200;
    const y = (parentNode?.y || 0);

    graph.nodes.push({ id, type, label, props, x, y, w: 180, h: 72, extraProps:{} });
    graph.edges.push({ from: parentId, to: id });
  }

  // The behavior XML has no Root wrapper — add a visual __Root__ node
  const rootId = allocId(null);
  graph.nodes.push({ id: rootId, type: '__Root__', label: 'Root', props:{}, x:400, y:60, w:180, h:72, extraProps:{} });

  let sibIdx = 0;
  for (const child of behavior.children) {
    if (child.tagName === 'node') {
      // If the first node IS a Root class, don't double-wrap
      const childClass = normType(child.getAttribute('class') || '');
      if (childClass === '__Root__') {
        parseNode(child, 0, sibIdx++, null);
      } else {
        parseNode(child, 1, sibIdx++, rootId);
      }
    }
  }

  autoLayout(graph);
}

function autoLayout(graph) {
  if (!graph.nodes.length) return;

  const childMap = {};
  const parentOf = {};
  for (const n of graph.nodes) childMap[n.id] = [];
  for (const e of graph.edges) {
    const child = graph.nodes.find(n => n.id === e.to);
    if (child && !isAttachment(child.type)) {
      childMap[e.from].push(e.to);
      parentOf[e.to] = e.from;
    }
  }

  const nodeById = Object.fromEntries(graph.nodes.map(n => [n.id, n]));
  const roots    = graph.nodes.filter(n => !parentOf[n.id] && !isAttachment(n.type));
  const DX = 210, DY = 140;

  function subW(id) {
    const kids = childMap[id] || [];
    return kids.length ? kids.reduce((s,k) => s + subW(k), 0) : DX;
  }

  function place(id, cx, y) {
    const n = nodeById[id];
    if (!n) return;
    const kids = childMap[id] || [];
    n.y = y;
    if (!kids.length) { n.x = cx - 90; return; }
    let startX = cx - subW(id) / 2;
    for (const kid of kids) {
      place(kid, startX + subW(kid) / 2, y + DY);
      startX += subW(kid);
    }
    n.x = cx - 90;
    // Reposition attachment nodes beside their parent
    const atts = graph.edges
      .filter(e => e.from === id)
      .map(e => graph.nodes.find(nn => nn.id === e.to))
      .filter(nn => nn && isAttachment(nn.type));
    atts.forEach((an, i) => { an.x = n.x + 195; an.y = n.y + i * 85; });
  }

  let sx = 400;
  for (const r of roots) {
    place(r.id, sx + subW(r.id) / 2, 60);
    sx += subW(r.id) + DX;
  }
}
