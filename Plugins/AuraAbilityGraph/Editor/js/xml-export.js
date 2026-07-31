// xml-export.js — Serialize graph to AuraAbilityGraph-compatible XML
// Wire format: <property name="Key" value="Val"/> inside node elements

function exportToXML(graph) {
  const treeName = graph.treeName || 'Ability_Unnamed';
  const nodes    = graph.nodes;
  const edges    = graph.edges;
  const meta     = graph.abilityMeta || {};

  const childMap  = {};
  const parentOf  = {};

  for (const n of nodes) { childMap[n.id] = []; }
  for (const e of edges) {
    (childMap[e.from] = childMap[e.from] || []).push(e.to);
    parentOf[e.to] = e.from;
  }

  for (const id in childMap) {
    childMap[id].sort((a, b) => (graph.nodes.find(n => n.id === a)?.x || 0) - (graph.nodes.find(n => n.id === b)?.x || 0));
  }

  const nodeById = Object.fromEntries(nodes.map(n => [n.id, n]));

  const lines = [];
  lines.push(`<?xml version="1.0" encoding="utf-8"?>`);

  const name = escXML(meta.name || treeName);
  const abilityTag = escXML(meta.abilityTag || '');
  const inputTag = escXML(meta.inputTag || '');
  const type = escXML(meta.type || 'Abilities.Active');

  lines.push(`<ability name="${name}" abilityTag="${abilityTag}" inputTag="${inputTag}" type="${type}">`);

  if (meta.mana !== undefined && meta.mana !== '') {
    lines.push(`  <cost mana="${escXML(String(meta.mana))}"/>`);
  }
  if (meta.cooldownTag || meta.cooldownDuration) {
    lines.push(`  <cooldown tag="${escXML(meta.cooldownTag || '')}" duration="${escXML(String(meta.cooldownDuration || '0'))}"/>`);
  }
  if (meta.damageEffectClass || meta.baseDamage) {
    const dmgAttrs = [
      `effectClass="${escXML(meta.damageEffectClass || '')}"`,
      `type="${escXML(meta.damageType || '')}"`,
      `base="${escXML(String(meta.baseDamage || '0'))}"`,
      `debuffChance="${escXML(String(meta.debuffChance || '0'))}"`,
      `debuffDamage="${escXML(String(meta.debuffDamage || '0'))}"`,
      `debuffDuration="${escXML(String(meta.debuffDuration || '0'))}"`,
      `debuffFrequency="${escXML(String(meta.debuffFrequency || '0'))}"`,
      `deathImpulseMagnitude="${escXML(String(meta.deathImpulse || '0'))}"`,
      `knockbackForceMagnitude="${escXML(String(meta.knockbackForce || '0'))}"`,
      `knockbackChance="${escXML(String(meta.knockbackChance || '0'))}"`,
    ].filter(Boolean).join(' ');
    lines.push(`  <damage ${dmgAttrs}/>`);
  }

  lines.push(`  <graph>`);

  const roots = nodes.filter(n => !parentOf[n.id] && isComposite(n.type));
  if (roots.length === 0 && nodes.length > 0) {
    const first = nodes[0];
    roots.push(first);
  }
  for (const root of roots) {
    writeNode(root.id, 2);
  }

  lines.push(`  </graph>`);
  lines.push(`</ability>`);
  return lines.join('\n');

  function writeNode(nid, indent) {
    const n = nodeById[nid];
    if (!n) return;
    const pad = '  '.repeat(indent);
    const props = buildProps(n);
    const kids  = childMap[nid] || [];

    if (!props.length && !kids.length) {
      lines.push(`${pad}<node class="${escXML(n.type)}"/>`);
    } else {
      lines.push(`${pad}<node class="${escXML(n.type)}">`);
      for (const p of props) {
        lines.push(`${pad}  <property name="${escXML(p.key)}" value="${escXML(p.value)}"/>`);
      }
      for (const kid of kids) {
        writeNode(kid, indent + 1);
      }
      lines.push(`${pad}</node>`);
    }
  }
}

function buildProps(n) {
  const schema = getNodeProps(n.type);
  const result = [];

  if (n.label && n.label !== n.type && n.label !== getDisplayLabel(n.type)) {
    result.push({ key: 'Name', value: n.label });
  }

  for (const s of schema) {
    const val = (n.props && n.props[s.key] !== undefined) ? n.props[s.key] : s.default;
    if (val !== '' && val !== undefined && val !== null) {
      if (s.type === 'bool' && val === false && s.default === false) continue;
      result.push({ key: s.key, value: s.type === 'bool' ? (val ? 'true' : 'false') : String(val) });
    }
  }
  if (n.extraProps) {
    for (const [k, v] of Object.entries(n.extraProps)) {
      if (k && k !== '__warning' && v !== '') result.push({ key: k, value: String(v) });
    }
  }
  return result;
}

function escXML(s) {
  return String(s ?? '').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}
