// xml-import.js — Parse AuraAbilityGraph XML into graph nodes/edges
// Supports <property name="Key" value="Val"/> format

function importFromXML(xmlStr, graph) {
  let doc;
  try {
    doc = new DOMParser().parseFromString(xmlStr, 'application/xml');
  } catch(e) { alert('XML parse error: ' + e.message); return; }
  const errNode = doc.querySelector('parsererror');
  if (errNode) { alert('Invalid XML:\n' + errNode.textContent.slice(0,300)); return; }

  const ability = doc.querySelector('ability');
  if (!ability) { alert('No <ability> element found.'); return; }

  graph.abilityMeta = graph.abilityMeta || {};
  graph.abilityMeta.name = ability.getAttribute('name') || 'Imported';
  graph.abilityMeta.abilityTag = ability.getAttribute('abilityTag') || '';
  graph.abilityMeta.inputTag = ability.getAttribute('inputTag') || '';
  graph.abilityMeta.type = ability.getAttribute('type') || 'Abilities.Active';

  for (const child of ability.children) {
    const tag = child.tagName;
    if (tag === 'cost') {
      graph.abilityMeta.mana = child.getAttribute('mana') || '';
    } else if (tag === 'cooldown') {
      graph.abilityMeta.cooldownTag = child.getAttribute('tag') || '';
      graph.abilityMeta.cooldownDuration = child.getAttribute('duration') || '';
    } else if (tag === 'damage') {
      graph.abilityMeta.damageEffectClass = child.getAttribute('effectClass') || '';
      graph.abilityMeta.damageType = child.getAttribute('type') || '';
      graph.abilityMeta.baseDamage = child.getAttribute('base') || '';
      graph.abilityMeta.debuffChance = child.getAttribute('debuffChance') || '';
      graph.abilityMeta.debuffDamage = child.getAttribute('debuffDamage') || '';
      graph.abilityMeta.debuffDuration = child.getAttribute('debuffDuration') || '';
      graph.abilityMeta.debuffFrequency = child.getAttribute('debuffFrequency') || '';
      graph.abilityMeta.deathImpulse = child.getAttribute('deathImpulseMagnitude') || '';
      graph.abilityMeta.knockbackForce = child.getAttribute('knockbackForceMagnitude') || '';
      graph.abilityMeta.knockbackChance = child.getAttribute('knockbackChance') || '';
    }
  }

  graph.treeName = graph.abilityMeta.name;
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

  function readProps(el) {
    const props = {};
    for (const propEl of el.children) {
      if (propEl.tagName !== 'property') continue;
      const nameAttr  = propEl.getAttribute('name');
      const valueAttr = propEl.getAttribute('value');
      if (nameAttr !== null) {
        props[normPropKey(nameAttr)] = valueAttr ?? '';
      }
    }
    return props;
  }

  function normPropKey(k) {
    return k;
  }

  function parseNode(el, depth, sibIdx, parentId) {
    const rawType = el.getAttribute('class') || el.tagName;
    const type    = rawType;
    const rawId   = el.getAttribute('id');
    const id      = allocId(rawId);

    const props = readProps(el);
    const label = props.Name || getDisplayLabel(type);
    delete props.Name;

    const x = sibIdx * 200 + 60;
    const y = depth  * 130 + 60;

    graph.nodes.push({ id, type, label, props, x, y, w: 180, h: 72, extraProps:{} });
    if (parentId !== null) graph.edges.push({ from: parentId, to: id });

    let childIdx = 0;
    for (const child of el.children) {
      if (child.tagName === 'node' || child.tagName === 'custom') {
        parseNode(child, depth + 1, childIdx++, id);
      }
    }
  }

  const graphEl = ability.querySelector('graph');
  if (graphEl) {
    let sibIdx = 0;
    for (const child of graphEl.children) {
      if (child.tagName === 'node' || child.tagName === 'custom') {
        parseNode(child, 0, sibIdx++, null);
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
    if (child) {
      childMap[e.from].push(e.to);
      parentOf[e.to] = e.from;
    }
  }

  const nodeById = Object.fromEntries(graph.nodes.map(n => [n.id, n]));
  const roots = graph.nodes.filter(n => !parentOf[n.id]);
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
  }

  let sx = 400;
  for (const r of roots) {
    place(r.id, sx + subW(r.id) / 2, 60);
    sx += subW(r.id) + DX;
  }
}
