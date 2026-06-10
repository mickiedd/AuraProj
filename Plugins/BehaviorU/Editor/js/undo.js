// undo.js — Command-based undo/redo stack

class UndoStack {
  constructor(limit = 50) {
    this._stack = [];
    this._pos   = -1;
    this._limit = limit;
  }
  push(snapshot) {
    // discard anything after current pos
    this._stack.splice(this._pos + 1);
    this._stack.push(snapshot);
    if (this._stack.length > this._limit) this._stack.shift();
    this._pos = this._stack.length - 1;
  }
  undo() {
    if (this._pos <= 0) return null;
    this._pos--;
    return this._stack[this._pos];
  }
  redo() {
    if (this._pos >= this._stack.length - 1) return null;
    this._pos++;
    return this._stack[this._pos];
  }
  canUndo() { return this._pos > 0; }
  canRedo() { return this._pos < this._stack.length - 1; }
  clear()   { this._stack = []; this._pos = -1; }
}
