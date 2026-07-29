// ability-data.js — AbilityInfo.json and RoleConfig.json helpers

const AbilityData = {
  async loadAbilityInfo() {
    try {
      const resp = await fetch('/load-ability-info', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{}' });
      const data = await resp.json();
      if (data.ok && data.content) {
        return JSON.parse(data.content);
      }
    } catch (e) { console.error('Failed to load ability info:', e); }
    return { abilities: [] };
  },

  async saveAbilityInfo(info) {
    try {
      const resp = await fetch('/save-ability-info', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ content: JSON.stringify(info, null, 2) }),
      });
      const data = await resp.json();
      return data.ok;
    } catch (e) { console.error('Failed to save ability info:', e); return false; }
  },

  async loadRoleConfig() {
    try {
      const resp = await fetch('/load-role-config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{}' });
      const data = await resp.json();
      if (data.ok && data.content) {
        return JSON.parse(data.content);
      }
    } catch (e) { console.error('Failed to load role config:', e); }
    return { roles: [] };
  },

  async saveRoleConfig(config) {
    try {
      const resp = await fetch('/save-role-config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ content: JSON.stringify(config, null, 2) }),
      });
      const data = await resp.json();
      return data.ok;
    } catch (e) { console.error('Failed to save role config:', e); return false; }
  },

  async loadGEConfig() {
    try {
      const resp = await fetch('/load-ge-config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{}' });
      const data = await resp.json();
      if (data.ok && data.content) {
        return { config: JSON.parse(data.content), path: data.path };
      }
    } catch (e) { console.error('Failed to load GE config:', e); }
    return { config: null, path: '' };
  },

  async saveGEConfig(config) {
    try {
      const resp = await fetch('/save-ge-config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ content: JSON.stringify(config, null, 2) }),
      });
      const data = await resp.json();
      return data.ok;
    } catch (e) { console.error('Failed to save GE config:', e); return false; }
  },
};
