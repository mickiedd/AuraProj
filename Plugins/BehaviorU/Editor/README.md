# BehaviorU — Behavior Tree Editor

A fully visual, browser-based Behavior Tree editor for the **BehaviacPlugin** UE5 plugin.

## Quick Start

Just open `index.html` in any modern browser — no server, no build step needed.

```bash
open /Volumes/M2/Works/BehaviorU/Editor/index.html
```

---

## Features

### Nodes
- All **41 node types** from the plugin: Composites, Actions, Conditions, Decorators, Attachments
- Color-coded by category (blue=composite, green=action, amber=condition, purple=decorator, red=attachment)
- Drag from palette → canvas to add nodes
- Double-click a node to rename it

### Connections
- Drag from the **bottom port** (circle) of a node to the **top port** of another to connect
- Attachment nodes connect via their **left port**
- Click an edge to select it, then press **Delete** to remove it

### Properties Panel
- Select any node to edit its type-specific properties in the right panel
- Supports string, number, dropdown, boolean fields
- Add custom key/value properties with the "+ Add Property" button

### Multi-tree Editing
- Click **+** in the tab bar or "New Tree" to add more trees
- Each tab is an independent behavior tree

### Auto Layout
- Click **Layout** in the toolbar to automatically arrange nodes top-down (BFS)
- Press **F** to fit all nodes to the screen

### Import / Export
- **Export XML** → generates behaviac-compatible XML (matches plugin's `LoadFromXML()`)
- **Import XML** → paste XML or load a `.xml` file
- **Download** saves the XML file

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Del` / `Backspace` | Delete selected nodes/edges |
| `Ctrl+Z` / `Cmd+Z` | Undo |
| `Ctrl+Y` / `Cmd+Shift+Z` | Redo |
| `Ctrl+A` / `Cmd+A` | Select all |
| `Ctrl+D` / `Cmd+D` | Duplicate selected |
| `Ctrl+S` / `Cmd+S` | Export XML |
| `Escape` | Deselect all |
| `F` | Fit all nodes to screen |
| `Space` + drag | Pan canvas |
| Scroll wheel | Zoom in/out |

## Workflow: From Editor to UE5

1. Design your tree in the editor
2. Click **Export XML** → **Download .xml**
3. In UE5 Content Browser: right-click → **Import** → select the `.xml` file
4. The plugin's `UBehaviacBehaviorTreeImportFactory` creates a `.uasset` automatically

## XML Format

The exported XML is compatible with the plugin's importer:

```xml
<behavior name="BT_NPC" agenttype="BTAgent" version="5">
  <node id="1" class="Root">
    <node id="2" class="Selector">
      <node id="3" class="Sequence">
        <node id="4" class="Condition">
          <property Opl="Self.health" Operator="Greater" Opr="50"/>
        </node>
        <node id="5" class="Action">
          <property MethodName="Attack"/>
        </node>
      </node>
    </node>
  </node>
</behavior>
```

## File Structure

```
Editor/
  index.html          ← Open this in your browser
  js/
    nodes.js          ← Node type definitions + property schemas
    graph.js          ← Canvas rendering (pan/zoom/hit-testing)
    xml-export.js     ← Serialize graph → XML
    xml-import.js     ← Parse XML → graph
    properties.js     ← Right-panel property editor
    undo.js           ← Undo/redo stack
    app.js            ← Main app logic
  css/
    style.css         ← Dark theme
  README.md           ← This file
```
