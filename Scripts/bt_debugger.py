"""Standalone Behavior Tree log debugger.

The tool is intentionally independent from Unreal and from the BehaviorU
editor.  It reads a captured or live Unreal log, discovers Behavior Tree names
from the messages that already exist, and lets the user decide which tree's
messages are displayed.

Usage examples::

    python Scripts/bt_debugger.py
    python Scripts/bt_debugger.py --log Saved/Logs/AuraServerFinal.log

This first version filters the debugger's display only.  It does not change
the server's logging verbosity or mutate the running game.
"""

from __future__ import annotations

import argparse
from collections import deque
from dataclasses import dataclass
from pathlib import Path
import re
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from typing import Iterable, Optional


UNATTRIBUTED_KEY = "__unattributed__"
UNATTRIBUTED_LABEL = "Unattributed BT messages"
RUNTIME_TREE_PREFIX = "BehaviorUBehaviorTree_"


@dataclass
class TreeInfo:
    """Aggregated information about a discovered tree or BT log source."""

    key: str
    label: str
    kind: str = "tree"
    line_count: int = 0
    first_line: int = 0
    last_line: int = 0
    enabled: bool = True


@dataclass(frozen=True)
class LogEntry:
    """One parsed line retained in the bounded display buffer."""

    line_number: int
    text: str
    keys: frozenset[str]


class BTLogParser:
    """Parse existing Unreal log conventions into stable display keys.

    Explicit ``BT=...`` and ``tree=...`` fields are preferred.  Contexts such
    as ``[CivilianAI][BT]`` are correlated to the most recently observed tree
    for that context or actor.  Task/service markers are kept as source keys,
    because the current server log does not always identify their owning tree.
    """

    _explicit_field_re = re.compile(
        r"\b(?:BT|tree)\s*=\s*([^\s\],;]+)", re.IGNORECASE
    )
    _tree_colon_re = re.compile(
        r"\b(?:behavior\s+tree|tree)\s*:\s*([^\s\],;]+)", re.IGNORECASE
    )
    _asset_name_re = re.compile(
        r"\b(BT_[A-Za-z0-9_]+)\.(?:xml|uasset)\b", re.IGNORECASE
    )
    _xml_path_re = re.compile(
        r"(?:auto-loading|behavior\s+tree(?:\s+from\s+xml\s+file)?)\s*[:=]?\s*"
        r"([^\s\],;]+\.xml)",
        re.IGNORECASE,
    )
    _entity_re = re.compile(
        r"\b(?:Enemy|Pawn|pawn|Actor|agent|Agent|controller|Controller)"
        r"=([^\s,;\]]+)"
    )
    _bracket_re = re.compile(r"\[([^\]]+)\]")
    _task_marker_re = re.compile(
        r"\[((?:BTTask|BTService|BTDecorator)_[^\]]+)\]", re.IGNORECASE
    )
    _bt_context_re = re.compile(r"\[([^\]]+)\]\[BT\]", re.IGNORECASE)

    def __init__(self) -> None:
        self.context_trees: dict[str, str] = {}
        self.entity_trees: dict[str, str] = {}

    @staticmethod
    def _clean_value(value: str) -> str:
        return value.strip().strip("'\"").rstrip(".,")

    @classmethod
    def normalize_tree_name(cls, value: str) -> Optional[str]:
        """Turn a log value or path into a user-facing tree name."""

        value = cls._clean_value(value)
        if not value or value.lower() in {"none", "null", "invalid"}:
            return None

        # Runtime UObject names are not useful BT identities.
        if value.startswith(RUNTIME_TREE_PREFIX):
            return None

        value = value.replace("\\", "/")
        if "/" in value:
            value = value.rsplit("/", 1)[-1]

        for suffix in (".xml", ".uasset"):
            if value.lower().endswith(suffix):
                value = value[: -len(suffix)]
                break

        return value or None

    @staticmethod
    def _source_key(label: str) -> str:
        return f"source:{label}"

    def _ensure_key(
        self,
        infos: dict[str, TreeInfo],
        key: str,
        label: Optional[str],
        kind: str,
        line_number: int,
    ) -> None:
        info = infos.get(key)
        if info is None:
            info = TreeInfo(
                key=key,
                label=label or key,
                kind=kind,
                first_line=line_number,
            )
            infos[key] = info
        info.line_count += 1
        info.last_line = line_number

    def parse(
        self,
        line: str,
        line_number: int,
        infos: dict[str, TreeInfo],
    ) -> LogEntry:
        explicit_trees: set[str] = set()

        for match in self._explicit_field_re.finditer(line):
            tree = self.normalize_tree_name(match.group(1))
            if tree:
                explicit_trees.add(tree)

        for match in self._tree_colon_re.finditer(line):
            tree = self.normalize_tree_name(match.group(1))
            if tree:
                explicit_trees.add(tree)

        # Asset and XML-path forms are only considered when the surrounding
        # line is clearly about behavior-tree loading/initialization.  This
        # avoids treating a generic BTS_ service asset warning as a BT.
        if "behavior tree" in line.lower() or "auto-loading" in line.lower():
            for match in self._asset_name_re.finditer(line):
                tree = self.normalize_tree_name(match.group(1))
                if tree:
                    explicit_trees.add(tree)
            for match in self._xml_path_re.finditer(line):
                tree = self.normalize_tree_name(match.group(1))
                if tree:
                    explicit_trees.add(tree)

        brackets = self._bracket_re.findall(line)
        contexts = [value for value in brackets if value.upper() != "BT"]
        bt_context_match = self._bt_context_re.search(line)
        bt_context = bt_context_match.group(1) if bt_context_match else None

        entities = [self._clean_value(match.group(1)) for match in self._entity_re.finditer(line)]
        entities = [value for value in entities if value]

        # Explicit names establish future correlation for context and entity
        # markers.  The first name is enough for the current one-tree-per-log
        # context convention; all explicit names remain attached to the line.
        primary_tree = sorted(explicit_trees)[0] if explicit_trees else None
        if primary_tree:
            for context in contexts:
                self.context_trees[context] = primary_tree
            if bt_context:
                self.context_trees[bt_context] = primary_tree
            for entity in entities:
                self.entity_trees[entity] = primary_tree

        effective_trees = set(explicit_trees)
        for context in contexts:
            mapped = self.context_trees.get(context)
            if mapped:
                effective_trees.add(mapped)
        if bt_context:
            mapped = self.context_trees.get(bt_context)
            if mapped:
                effective_trees.add(mapped)
        for entity in entities:
            mapped = self.entity_trees.get(entity)
            if mapped:
                effective_trees.add(mapped)

        keys: set[str] = set()
        for tree in effective_trees:
            keys.add(tree)
            self._ensure_key(infos, tree, tree, "tree", line_number)

        # Preserve a useful source-level toggle for task/service/decorator
        # logs.  If a tree is known, the tree toggle is sufficient; the source
        # key is still added so the user can focus on one task family.
        for match in self._task_marker_re.finditer(line):
            source_label = match.group(1)
            source_key = self._source_key(source_label)
            keys.add(source_key)
            self._ensure_key(infos, source_key, source_label, "source", line_number)

        if bt_context and not effective_trees:
            source_label = f"[{bt_context}][BT]"
            source_key = self._source_key(source_label)
            keys.add(source_key)
            self._ensure_key(infos, source_key, source_label, "source", line_number)

        if not keys and (" BT" in line or "BT=" in line or "tree=" in line):
            keys.add(UNATTRIBUTED_KEY)
            self._ensure_key(infos, UNATTRIBUTED_KEY, UNATTRIBUTED_LABEL, "unattributed", line_number)

        # Any task/source line without a tree must remain discoverable under a
        # single aggregate row too.
        if keys and not effective_trees and any(key.startswith("source:") for key in keys):
            keys.add(UNATTRIBUTED_KEY)
            self._ensure_key(infos, UNATTRIBUTED_KEY, UNATTRIBUTED_LABEL, "unattributed", line_number)

        return LogEntry(line_number=line_number, text=line, keys=frozenset(keys))


class LogModel:
    """File loader, live tailer, and bounded parsed-entry store."""

    def __init__(self, max_lines: int = 5000) -> None:
        self.max_lines = max(100, max_lines)
        self.path: Optional[Path] = None
        self.entries: deque[LogEntry] = deque(maxlen=self.max_lines)
        self.infos: dict[str, TreeInfo] = {}
        self.parser = BTLogParser()
        self._offset = 0
        self._line_number = 0
        self._pending = ""
        self._file_identity: tuple[int, int] | None = None

    def clear(self) -> None:
        self.entries.clear()
        self.infos.clear()
        self.parser = BTLogParser()
        self._offset = 0
        self._line_number = 0
        self._pending = ""
        self._file_identity = None

    @staticmethod
    def _get_file_identity(path: Path) -> tuple[int, int] | None:
        try:
            metadata = path.stat()
        except OSError:
            return None
        return metadata.st_dev, metadata.st_ino

    @property
    def file_size(self) -> int:
        if not self.path:
            return 0
        try:
            return self.path.stat().st_size
        except OSError:
            return 0

    def load(self, path: Path) -> None:
        path = path.expanduser().resolve()
        self.clear()
        self.path = path
        if not path.is_file():
            return

        self._file_identity = self._get_file_identity(path)
        with path.open("r", encoding="utf-8", errors="replace", newline="") as handle:
            for raw_line in handle:
                self._ingest_line(raw_line.rstrip("\r\n"))
            self._offset = handle.tell()

    def _ingest_line(self, line: str) -> None:
        self._line_number += 1
        self.entries.append(self.parser.parse(line, self._line_number, self.infos))

    def poll(self) -> bool:
        """Read complete appended lines; return whether the model changed."""

        if not self.path or not self.path.is_file():
            return False

        try:
            metadata = self.path.stat()
        except OSError:
            return False
        size = metadata.st_size
        file_identity = (metadata.st_dev, metadata.st_ino)

        # A restarted server may truncate or replace the file.  Re-scan so
        # stale tree names and old entries do not leak into the new session.
        if (self._file_identity is not None and file_identity != self._file_identity) or size < self._offset:
            self.load(self.path)
            return True

        if size == self._offset:
            return False

        changed = False
        with self.path.open("r", encoding="utf-8", errors="replace", newline="") as handle:
            handle.seek(self._offset)
            chunk = handle.read()
            self._offset = handle.tell()

        if not chunk:
            return False

        combined = self._pending + chunk
        parts = combined.splitlines(keepends=True)
        self._pending = ""
        if parts and not parts[-1].endswith(("\n", "\r")):
            self._pending = parts.pop()

        for raw_line in parts:
            self._ingest_line(raw_line.rstrip("\r\n"))
            changed = True
        return changed

    def sorted_infos(self) -> list[TreeInfo]:
        kind_order = {"tree": 0, "source": 1, "unattributed": 2}
        return sorted(
            self.infos.values(),
            key=lambda info: (kind_order.get(info.kind, 9), info.label.lower()),
        )

    def visible_entries(self, selected_key: Optional[str], all_enabled: bool, query: str) -> list[LogEntry]:
        query = query.strip().lower()
        if all_enabled:
            enabled_keys = {key for key, info in self.infos.items() if info.enabled}
            candidates = [
                entry for entry in self.entries if entry.keys.intersection(enabled_keys)
            ]
        elif selected_key:
            info = self.infos.get(selected_key)
            candidates = [
                entry
                for entry in self.entries
                if info is not None and info.enabled and selected_key in entry.keys
            ]
        else:
            candidates = []

        if query:
            candidates = [entry for entry in candidates if query in entry.text.lower()]
        return candidates


def discover_log_candidates(project_root: Path) -> list[Path]:
    """Find likely server logs, preferring names that contain ``Server``."""

    log_dir = project_root / "Saved" / "Logs"
    if not log_dir.is_dir():
        return []

    files = [path for path in log_dir.glob("*.log") if path.is_file()]
    server_files = [path for path in files if "server" in path.name.lower()]
    candidates = server_files or [path for path in files if path.name.lower() == "aura.log"] or files
    return sorted(candidates, key=lambda path: path.stat().st_mtime, reverse=True)


class BTDebuggerApp:
    """Tkinter presentation for the standalone debugger."""

    def __init__(self, root: tk.Tk, project_root: Path, initial_log: Optional[Path], max_lines: int, follow: bool) -> None:
        self.root = root
        self.project_root = project_root
        self.model = LogModel(max_lines=max_lines)
        self.selected_key: Optional[str] = None
        self._tree_iids: dict[str, str] = {}
        self._refreshing_tree = False

        self.follow_var = tk.BooleanVar(value=follow)
        self.view_mode_var = tk.StringVar(value="all")
        self.selected_display_var = tk.BooleanVar(value=True)
        self.query_var = tk.StringVar()
        self.status_var = tk.StringVar(value="No log loaded")
        self.summary_var = tk.StringVar(value="0 discovered BT sources")
        self.selected_var = tk.StringVar(value="Select a BT/source to inspect it")

        self._configure_style()
        self._build_ui()
        self._bind_events()

        candidates = discover_log_candidates(project_root)
        path = initial_log or (candidates[0] if candidates else None)
        if path:
            self.load_log(path)
        else:
            self._render_all()
        self._schedule_poll()

    def _configure_style(self) -> None:
        self.root.title("Aura BT Debugger")
        self.root.geometry("1280x760")
        self.root.minsize(900, 520)
        style = ttk.Style(self.root)
        try:
            style.theme_use("clam")
        except tk.TclError:
            pass
        style.configure("Title.TLabel", font=("Segoe UI", 15, "bold"))
        style.configure("Muted.TLabel", foreground="#68717f")
        style.configure("Treeview", rowheight=26)

    def _build_ui(self) -> None:
        outer = ttk.Frame(self.root, padding=12)
        outer.pack(fill="both", expand=True)

        header = ttk.Frame(outer)
        header.pack(fill="x", pady=(0, 10))
        ttk.Label(header, text="Aura BT Debugger", style="Title.TLabel").pack(side="left")
        ttk.Label(
            header,
            text="Display filter for Unreal server logs · server behavior is unchanged",
            style="Muted.TLabel",
        ).pack(side="left", padx=(14, 0))

        source_bar = ttk.Frame(outer)
        source_bar.pack(fill="x", pady=(0, 8))
        ttk.Label(source_bar, text="Log file:").pack(side="left")
        self.path_label = ttk.Label(source_bar, text="(none)", style="Muted.TLabel")
        self.path_label.pack(side="left", padx=(6, 12), fill="x", expand=True)
        ttk.Button(source_bar, text="Open…", command=self.choose_log).pack(side="left", padx=3)
        ttk.Button(source_bar, text="Refresh", command=self.reload_log).pack(side="left", padx=3)
        ttk.Checkbutton(source_bar, text="Follow live", variable=self.follow_var).pack(side="left", padx=(10, 0))

        body = ttk.PanedWindow(outer, orient="horizontal")
        body.pack(fill="both", expand=True)

        left = ttk.Frame(body, padding=(0, 0, 8, 0))
        right = ttk.Frame(body, padding=(8, 0, 0, 0))
        body.add(left, weight=1)
        body.add(right, weight=4)

        left_header = ttk.Frame(left)
        left_header.pack(fill="x", pady=(0, 6))
        ttk.Label(left_header, text="BTs and log sources").pack(side="left")
        ttk.Label(left_header, textvariable=self.summary_var, style="Muted.TLabel").pack(side="right")

        tree_frame = ttk.Frame(left)
        tree_frame.pack(fill="both", expand=True)
        self.bt_tree = ttk.Treeview(
            tree_frame,
            columns=("display", "name", "kind", "lines"),
            show="headings",
            selectmode="browse",
        )
        self.bt_tree.heading("display", text="Show")
        self.bt_tree.heading("name", text="BT / source")
        self.bt_tree.heading("kind", text="Kind")
        self.bt_tree.heading("lines", text="Lines")
        self.bt_tree.column("display", width=56, minwidth=56, anchor="center", stretch=False)
        self.bt_tree.column("name", width=230, minwidth=140, anchor="w")
        self.bt_tree.column("kind", width=88, minwidth=70, anchor="center", stretch=False)
        self.bt_tree.column("lines", width=72, minwidth=55, anchor="e", stretch=False)
        tree_scroll = ttk.Scrollbar(tree_frame, orient="vertical", command=self.bt_tree.yview)
        self.bt_tree.configure(yscrollcommand=tree_scroll.set)
        self.bt_tree.pack(side="left", fill="both", expand=True)
        tree_scroll.pack(side="right", fill="y")

        actions = ttk.Frame(left)
        actions.pack(fill="x", pady=(8, 0))
        ttk.Button(actions, text="Show all", command=lambda: self.set_all_enabled(True)).pack(side="left")
        ttk.Button(actions, text="Hide all", command=lambda: self.set_all_enabled(False)).pack(side="left", padx=(5, 0))

        ttk.Separator(right, orient="horizontal").pack(fill="x", pady=(0, 8))
        selection_bar = ttk.Frame(right)
        selection_bar.pack(fill="x", pady=(0, 8))
        ttk.Label(selection_bar, textvariable=self.selected_var).pack(side="left")
        ttk.Checkbutton(
            selection_bar,
            text="Display selected log",
            variable=self.selected_display_var,
            command=self.toggle_selected,
        ).pack(side="right")

        view_bar = ttk.Frame(right)
        view_bar.pack(fill="x", pady=(0, 8))
        ttk.Label(view_bar, text="View:").pack(side="left")
        ttk.Radiobutton(view_bar, text="All enabled", variable=self.view_mode_var, value="all", command=self.render_log).pack(side="left", padx=(8, 0))
        ttk.Radiobutton(view_bar, text="Selected only", variable=self.view_mode_var, value="selected", command=self.render_log).pack(side="left", padx=(8, 0))
        ttk.Label(view_bar, text="Search:").pack(side="left", padx=(18, 4))
        search_entry = ttk.Entry(view_bar, textvariable=self.query_var, width=28)
        search_entry.pack(side="left")
        ttk.Button(view_bar, text="Clear", command=lambda: self.query_var.set("")).pack(side="left", padx=(4, 0))

        log_frame = ttk.Frame(right)
        log_frame.pack(fill="both", expand=True)
        self.log_text = tk.Text(
            log_frame,
            wrap="none",
            state="disabled",
            font=("Consolas", 10),
            background="#111827",
            foreground="#d6deeb",
            insertbackground="#ffffff",
            selectbackground="#334155",
            relief="flat",
            padx=8,
            pady=8,
        )
        log_y = ttk.Scrollbar(log_frame, orient="vertical", command=self.log_text.yview)
        log_x = ttk.Scrollbar(log_frame, orient="horizontal", command=self.log_text.xview)
        self.log_text.configure(yscrollcommand=log_y.set, xscrollcommand=log_x.set)
        self.log_text.tag_configure("warning", foreground="#f6c453")
        self.log_text.tag_configure("error", foreground="#ff7b72")
        self.log_text.tag_configure("display", foreground="#7dd3fc")
        self.log_text.pack(side="left", fill="both", expand=True)
        log_y.pack(side="right", fill="y")
        log_x.pack(side="bottom", fill="x")

        footer = ttk.Frame(outer)
        footer.pack(fill="x", pady=(8, 0))
        ttk.Label(footer, textvariable=self.status_var, style="Muted.TLabel").pack(side="left")
        ttk.Label(
            footer,
            text="Click a row to select it · click Show or use the switch to hide/show its messages",
            style="Muted.TLabel",
        ).pack(side="right")

    def _bind_events(self) -> None:
        self.bt_tree.bind("<<TreeviewSelect>>", self._on_tree_selected)
        self.bt_tree.bind("<Button-1>", self._on_tree_click)
        self.query_var.trace_add("write", lambda *_args: self.render_log())

    def choose_log(self) -> None:
        initial_dir = str(self.model.path.parent if self.model.path else self.project_root / "Saved" / "Logs")
        filename = filedialog.askopenfilename(
            title="Open Unreal server log",
            initialdir=initial_dir,
            filetypes=[("Log files", "*.log"), ("All files", "*.*")],
        )
        if filename:
            self.load_log(Path(filename))

    def load_log(self, path: Path) -> None:
        try:
            self.model.load(path)
        except OSError as exc:
            messagebox.showerror("BT Debugger", f"Could not read log file:\n{path}\n\n{exc}")
            return

        self.path_label.configure(text=str(path))
        self.selected_key = None
        self._render_all()

    def reload_log(self) -> None:
        if self.model.path:
            self.load_log(self.model.path)
        else:
            candidates = discover_log_candidates(self.project_root)
            if candidates:
                self.load_log(candidates[0])

    def _schedule_poll(self) -> None:
        self.root.after(500, self._poll)

    def _poll(self) -> None:
        if self.follow_var.get() and self.model.poll():
            self._render_all()
        self._schedule_poll()

    def _render_all(self) -> None:
        self._render_tree_list()
        self._sync_selected_control()
        self.render_log()
        path_label = str(self.model.path) if self.model.path else "No log loaded"
        self.status_var.set(
            f"{path_label} · {self.model.file_size:,} bytes · {len(self.model.entries):,} recent lines retained"
        )

    def _render_tree_list(self) -> None:
        selected = self.selected_key
        self._refreshing_tree = True
        try:
            self.bt_tree.delete(*self.bt_tree.get_children())
            self._tree_iids.clear()
            infos = self.model.sorted_infos()
            tree_count = sum(info.kind == "tree" for info in infos)
            self.summary_var.set(f"{tree_count} BTs · {len(infos)} sources")
            for index, info in enumerate(infos):
                iid = f"bt-{index}"
                self._tree_iids[info.key] = iid
                display = "✓" if info.enabled else "—"
                kind = {"tree": "tree", "source": "source", "unattributed": "aggregate"}.get(info.kind, info.kind)
                self.bt_tree.insert("", "end", iid=iid, values=(display, info.label, kind, f"{info.line_count:,}"))

            if selected in self._tree_iids:
                self.bt_tree.selection_set(self._tree_iids[selected])
                self.bt_tree.focus(self._tree_iids[selected])
            elif infos:
                self.selected_key = infos[0].key
                self.bt_tree.selection_set(self._tree_iids[self.selected_key])
                self.bt_tree.focus(self._tree_iids[self.selected_key])
            else:
                self.selected_key = None
        finally:
            self._refreshing_tree = False

    def _on_tree_click(self, event: tk.Event) -> Optional[str]:
        row = self.bt_tree.identify_row(event.y)
        column = self.bt_tree.identify_column(event.x)
        if row and column == "#1":
            for key, iid in self._tree_iids.items():
                if iid == row:
                    self.model.infos[key].enabled = not self.model.infos[key].enabled
                    self._render_tree_list()
                    self._sync_selected_control()
                    self.render_log()
                    return "break"
        return None

    def _on_tree_selected(self, _event: tk.Event) -> None:
        if self._refreshing_tree:
            return
        selection = self.bt_tree.selection()
        if not selection:
            return
        iid = selection[0]
        self.selected_key = next((key for key, value in self._tree_iids.items() if value == iid), None)
        self._sync_selected_control()
        self.render_log()

    def _sync_selected_control(self) -> None:
        info = self.model.infos.get(self.selected_key or "")
        if info:
            self.selected_var.set(f"Selected: {info.label} · {info.kind} · {info.line_count:,} matching lines")
            self.selected_display_var.set(info.enabled)
        else:
            self.selected_var.set("Select a BT/source to inspect it")
            self.selected_display_var.set(False)

    def toggle_selected(self) -> None:
        info = self.model.infos.get(self.selected_key or "")
        if info:
            info.enabled = self.selected_display_var.get()
            self._render_tree_list()
            self.render_log()

    def set_all_enabled(self, enabled: bool) -> None:
        for info in self.model.infos.values():
            info.enabled = enabled
        self._render_tree_list()
        self._sync_selected_control()
        self.render_log()

    def render_log(self) -> None:
        all_enabled = self.view_mode_var.get() == "all"
        entries = self.model.visible_entries(self.selected_key, all_enabled, self.query_var.get())
        at_bottom = self.log_text.yview()[1] >= 0.98

        self.log_text.configure(state="normal")
        self.log_text.delete("1.0", "end")
        for entry in entries:
            text = f"{entry.line_number:>7}  {entry.text}\n"
            lowered = entry.text.lower()
            tag = "error" if " error:" in lowered or "fatal error" in lowered else "warning" if " warning:" in lowered else "display"
            self.log_text.insert("end", text, tag)
        self.log_text.configure(state="disabled")

        if at_bottom or self.follow_var.get():
            self.log_text.see("end")


def _parse_args(argv: Optional[Iterable[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Standalone BT log discovery and display filter.")
    parser.add_argument(
        "--log",
        type=Path,
        help="server log to open; defaults to the newest Saved/Logs/*Server*.log",
    )
    parser.add_argument(
        "--project-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="Aura project root used for default log discovery",
    )
    parser.add_argument(
        "--max-lines",
        type=int,
        default=5000,
        help="maximum recent lines kept in the display buffer (default: 5000)",
    )
    parser.add_argument(
        "--no-follow",
        action="store_true",
        help="open the file without polling for appended lines",
    )
    return parser.parse_args(argv)


def main(argv: Optional[Iterable[str]] = None) -> int:
    args = _parse_args(argv)
    project_root = args.project_root.expanduser().resolve()
    initial_log = args.log
    if initial_log and not initial_log.is_absolute():
        initial_log = (Path.cwd() / initial_log).resolve()

    root = tk.Tk()
    BTDebuggerApp(
        root,
        project_root=project_root,
        initial_log=initial_log,
        max_lines=args.max_lines,
        follow=not args.no_follow,
    )
    root.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
