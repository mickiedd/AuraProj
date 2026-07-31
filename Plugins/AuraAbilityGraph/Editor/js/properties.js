// properties.js — Right panel property editor

class PropertiesPanel {
  constructor(container, onChanged) {
    this.container  = container;
    this.onChanged  = onChanged;
    this._nodeId    = null;
  }

  show(node, abilityMeta) {
    this._nodeId = node ? node.id : null;
    this._abilityMeta = abilityMeta || {};
    this.container.innerHTML = '';

    if (node) {
      this._showNodeProps(node);
    } else if (abilityMeta) {
      this._showAbilityMeta(abilityMeta);
    } else {
      this.container.innerHTML = '<div id="props-empty">Select a node or ability to edit its properties.</div>';
    }
  }

  _showNodeProps(node) {
    const info = NODE_TYPE_MAP[node.type] || { category: 'composite', label: node.type };
    const col  = getCatColor(info.category);

    const badge = document.createElement('div');
    badge.className = 'prop-node-type';
    badge.textContent = node.type;
    badge.style.background = col.header;
    this.container.appendChild(badge);

    this._addRow(node, '__label', 'Display Name', { type: 'string' }, node.label || node.type);

    const schema = getNodeProps(node.type);
    for (const s of schema) {
      const cur = node.props?.[s.key] ?? s.default;
      this._addRow(node, s.key, s.label, s, cur);
    }

    this._addExtraPropsSection(node);
  }

  _showAbilityMeta(meta) {
    const badge = document.createElement('div');
    badge.className = 'prop-node-type';
    badge.textContent = 'Ability Metadata';
    badge.style.background = '#17a589';
    this.container.appendChild(badge);

    const fields = [
      { key:'name', label:'Ability Name', type:'string' },
      { key:'abilityTag', label:'Ability Tag', type:'string' },
      { key:'inputTag', label:'Input Tag', type:'string' },
      { key:'type', label:'Type', type:'string' },
      { key:'mana', label:'Mana Cost', type:'string' },
      { key:'cooldownTag', label:'Cooldown Tag', type:'string' },
      { key:'cooldownDuration', label:'Cooldown Duration', type:'string' },
      { key:'damageEffectClass', label:'Damage Effect Class', type:'string' },
      { key:'damageType', label:'Damage Type', type:'string' },
      { key:'baseDamage', label:'Base Damage', type:'string' },
      { key:'debuffChance', label:'Debuff Chance', type:'string' },
      { key:'debuffDamage', label:'Debuff Damage', type:'string' },
      { key:'debuffDuration', label:'Debuff Duration', type:'string' },
      { key:'debuffFrequency', label:'Debuff Frequency', type:'string' },
      { key:'deathImpulse', label:'Death Impulse Magnitude', type:'string' },
      { key:'knockbackForce', label:'Knockback Force Magnitude', type:'string' },
      { key:'knockbackChance', label:'Knockback Chance', type:'string' },
    ];

    for (const f of fields) {
      const val = meta[f.key] ?? '';
      this._addRow(null, f.key, f.label, { type: f.type }, val, true);
    }
  }

  _addRow(node, key, label, schema, currentVal, isMeta) {
    const row = document.createElement('div');
    row.className = 'prop-row';
    if (isMeta) row.dataset.metaKey = key;

    const lbl = document.createElement('div');
    lbl.className = 'prop-label';
    lbl.textContent = label;
    row.appendChild(lbl);

    let input;
    if (schema.type === 'bool') {
      const wrapper = document.createElement('div');
      wrapper.className = 'prop-checkbox-row';
      input = document.createElement('input');
      input.type = 'checkbox';
      input.checked = currentVal === true || currentVal === 'true';
      const span = document.createElement('span');
      span.style.fontSize = '12px';
      span.textContent = label;
      wrapper.appendChild(input);
      wrapper.appendChild(span);
      row.appendChild(wrapper);
    } else if (schema.type === 'select') {
      input = document.createElement('select');
      input.className = 'prop-select';
      for (const opt of (schema.options || [])) {
        const o = document.createElement('option');
        o.value = o.textContent = opt;
        if (opt === String(currentVal)) o.selected = true;
        input.appendChild(o);
      }
      row.appendChild(input);
    } else {
      input = document.createElement('input');
      input.className = 'prop-input';
      input.type = 'text';
      input.value = currentVal ?? '';
      row.appendChild(input);
    }

    const self = this;
    const onChange = () => {
      let val;
      if (schema.type === 'bool') val = input.checked;
      else val = input.value;
      if (isMeta) {
        const meta = self._currentMeta();
        if (meta) { meta[key] = val; self.onChanged(null, meta); }
      } else {
        if (key === '__label') {
          node.label = val;
        } else {
          if (!node.props) node.props = {};
          node.props[key] = val;
        }
        self.onChanged(node.id, node.props);
      }
    };

    if (schema.type === 'bool') input.addEventListener('change', onChange);
    else input.addEventListener('input', onChange);

    this.container.appendChild(row);
  }

  _addExtraPropsSection(node) {
    const extraHeader = document.createElement('div');
    extraHeader.className = 'prop-label';
    extraHeader.style.marginTop = '14px';
    extraHeader.style.borderTop = '1px solid var(--border)';
    extraHeader.style.paddingTop = '10px';
    extraHeader.textContent = 'Custom Properties';
    this.container.appendChild(extraHeader);

    const extra = node.extraProps || {};
    for (const [k, v] of Object.entries(extra)) {
      this._addExtraRow(node, k, v);
    }
    this._addNewExtraRow(node);
  }

  _addExtraRow(node, key, val) {
    const row = document.createElement('div');
    row.className = 'prop-row';
    row.style.display = 'flex'; row.style.gap = '4px';

    const keyInput = document.createElement('input');
    keyInput.className = 'prop-input'; keyInput.value = key;
    keyInput.style.flex = '1'; keyInput.placeholder = 'key';

    const valInput = document.createElement('input');
    valInput.className = 'prop-input'; valInput.value = val;
    valInput.style.flex = '1'; valInput.placeholder = 'value';

    const delBtn = document.createElement('button');
    delBtn.className = 'btn'; delBtn.textContent = '✕';
    delBtn.style.padding = '3px 7px'; delBtn.title = 'Remove property';

    const self = this;
    const update = () => {
      const oldKey = key;
      const newKey = keyInput.value.trim();
      const newVal = valInput.value;
      if (!node.extraProps) node.extraProps = {};
      delete node.extraProps[oldKey];
      if (newKey) node.extraProps[newKey] = newVal;
      key = newKey;
      self.onChanged(node.id, node.props);
    };
    keyInput.addEventListener('input', update);
    valInput.addEventListener('input', update);
    delBtn.addEventListener('click', () => {
      if (node.extraProps) delete node.extraProps[key];
      row.remove();
      self.onChanged(node.id, node.props);
    });

    row.appendChild(keyInput); row.appendChild(valInput); row.appendChild(delBtn);
    this.container.appendChild(row);
  }

  _addNewExtraRow(node) {
    const addBtn = document.createElement('button');
    addBtn.className = 'btn'; addBtn.textContent = '+ Add Property';
    addBtn.style.marginTop = '6px';
    addBtn.addEventListener('click', () => {
      if (!node.extraProps) node.extraProps = {};
      node.extraProps[''] = '';
      this.show(node);
    });
    this.container.appendChild(addBtn);
  }

  _currentMeta() {
    this._abilityMeta = this._abilityMeta || {};
    return this._abilityMeta;
  }

  clear() { this.show(null); }
}
