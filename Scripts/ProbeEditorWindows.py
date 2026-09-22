"""Enumerate the Unreal Editor's top-level windows to look for a modal dialog.

The editor's main thread is blocked with zero CPU consumption, which is the
signature of a modal dialog waiting for a human. `Add-Type` (the usual way to do
this from PowerShell) is blocked in this environment, so this goes through
ctypes instead.

Reports every top-level window owned by the process with its title, class,
visibility and enabled state. A modal dialog shows up as a visible, enabled
window of class #32770 while the editor's main window is disabled.
"""

import ctypes
import ctypes.wintypes as wintypes
import sys

PID = int(sys.argv[1]) if len(sys.argv) > 1 else 13636

user32 = ctypes.WinDLL("user32", use_last_error=True)

WNDENUMPROC = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

user32.EnumWindows.argtypes = [WNDENUMPROC, wintypes.LPARAM]
user32.EnumWindows.restype = wintypes.BOOL
user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND,
                                            ctypes.POINTER(wintypes.DWORD)]
user32.GetWindowThreadProcessId.restype = wintypes.DWORD
user32.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user32.GetWindowTextW.restype = ctypes.c_int
user32.GetClassNameW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user32.GetClassNameW.restype = ctypes.c_int
user32.IsWindowVisible.argtypes = [wintypes.HWND]
user32.IsWindowVisible.restype = wintypes.BOOL
user32.IsWindowEnabled.argtypes = [wintypes.HWND]
user32.IsWindowEnabled.restype = wintypes.BOOL
user32.GetWindowLongW.argtypes = [wintypes.HWND, ctypes.c_int]
user32.GetWindowLongW.restype = wintypes.LONG

WS_DISABLED = 0x08000000
GWL_STYLE = -16

rows = []


def callback(hwnd, lparam):
    pid = wintypes.DWORD()
    user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
    if pid.value == PID:
        title = ctypes.create_unicode_buffer(512)
        user32.GetWindowTextW(hwnd, title, 512)
        cls = ctypes.create_unicode_buffer(512)
        user32.GetClassNameW(hwnd, cls, 512)
        style = user32.GetWindowLongW(hwnd, GWL_STYLE)
        rows.append({
            "hwnd": hex(hwnd),
            "class": cls.value,
            "title": title.value,
            "visible": bool(user32.IsWindowVisible(hwnd)),
            "enabled": bool(user32.IsWindowEnabled(hwnd)),
            "ws_disabled": bool(style & WS_DISABLED),
        })
    return True


user32.EnumWindows(WNDENUMPROC(callback), 0)

print("WINDOW_COUNT", len(rows))
for row in rows:
    print("WINDOW", row)

dialogs = [row for row in rows
           if row["visible"] and row["class"] == "#32770"]
print("MODAL_DIALOG_CANDIDATES", len(dialogs))
disabled_mains = [row for row in rows if row["visible"] and row["ws_disabled"]]
print("DISABLED_VISIBLE_WINDOWS", len(disabled_mains))
for row in disabled_mains:
    print("DISABLED", row)
