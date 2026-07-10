// BehaviorULauncher.cpp
//
// Standalone Windows launcher for the BehaviorU Behavior Tree Editor.
//   1. Starts  Plugins/BehaviorUPlugin/Editor/behavioru_server.py  on port 18102.
//   2. Waits for the server to respond (up to 8 s).
//   3. Opens the web app in the system default browser (Edge/Chrome/etc.).
//   4. Shows a compact status window that keeps the server alive.
//   5. On window close: terminates the Python process and all resources.
//
// NOTE: An embedded-browser variant (using Microsoft WebView2) can be added by
//       installing the NuGet package 'Microsoft.Web.WebView2' and including WebView2.h.
//       This version uses only standard Win32/Winsock APIs - no NuGet required.
//
// Build:  run build.bat
// Output: Tools/BehaviorULauncher/BehaviorULauncher.exe

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "resource.h"
#include <shellapi.h>
#include <commctrl.h>
#include <objbase.h>
#include <string>
#include <ctime>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#ifdef WEBVIEW2_ENABLED
#include <unknwn.h>   // defines 'interface' macro (needed when WIN32_LEAN_AND_MEAN is set)
#include "WebView2.h"
#include <wrl.h>
using namespace Microsoft::WRL;
#endif

// -----------------------------------------------------------------------------
// Constants
// -----------------------------------------------------------------------------
static constexpr int    kHttpPort      = 18102;
static constexpr int    kWsPort        = 17654;
static constexpr UINT   kTimerPoll     = 1;
static constexpr UINT   kPollIntervalMs = 3000;   // check server alive every 3 s

static constexpr UINT   WM_SERVER_READY   = WM_APP + 1;  // wParam=1 success, 0 timeout
static constexpr UINT   WM_SERVER_SKIP    = WM_APP + 2;  // server already running
static constexpr UINT   WM_WEBVIEW2_READY = WM_APP + 3;  // WebView2 controller created
static constexpr UINT   WM_WEBVIEW2_FAIL  = WM_APP + 4;  // WebView2 unavailable

static constexpr int    kToolbarHeight    = 0;            // no toolbar; menu bar used instead

static const WCHAR kWindowClass[] = L"BehaviorULauncherWnd";
static const WCHAR kWindowTitle[] = L"BehaviorU - Behavior Tree Editor Launcher";

// Control / menu IDs
enum : int { ID_MENU_RESTART = 101, ID_MENU_LOG = 104 };

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------
static HANDLE      g_pythonProcess = INVALID_HANDLE_VALUE;
static HWND        g_hwnd          = nullptr;
static HMENU       g_hMenu         = nullptr;
static std::wstring g_editorDir;
static std::wstring g_navigateUrl;
static std::wstring g_serverLogPath;

#ifdef WEBVIEW2_ENABLED
static ICoreWebView2Controller* g_wv2Controller = nullptr;
static ICoreWebView2*           g_wv2            = nullptr;
static bool                     g_wv2Ready       = false;
static std::wstring             g_pendingUrl;     // URL to navigate once WebView2 is up
#endif

// -----------------------------------------------------------------------------
// Path utilities
// -----------------------------------------------------------------------------
static std::wstring GetExeDir()
{
    WCHAR buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf);
    const auto pos = p.rfind(L'\\');
    return (pos != std::wstring::npos) ? p.substr(0, pos) : p;
}

static std::wstring ResolvePath(const std::wstring& base, const wchar_t* relative)
{
    WCHAR full[MAX_PATH] = {};
    std::wstring combined = base + L"\\" + relative;
    GetFullPathNameW(combined.c_str(), MAX_PATH, full, nullptr);
    return std::wstring(full);
}

static bool PathExists(const std::wstring& path)
{
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// -----------------------------------------------------------------------------
// Network helpers
// -----------------------------------------------------------------------------
static bool IsPortOpen(int port)
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;

    u_long nonblocking = 1;
    ioctlsocket(s, FIONBIO, &nonblocking);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<u_short>(port));
    InetPtonW(AF_INET, L"127.0.0.1", &addr.sin_addr);
    connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(s, &fds);
    timeval tv{ 0, 300'000 };
    const bool open = select(0, nullptr, &fds, nullptr, &tv) > 0;
    closesocket(s);
    return open;
}

static bool WaitForPort(int port, int timeoutMs)
{
    for (int elapsed = 0; elapsed < timeoutMs; elapsed += 200)
    {
        if (IsPortOpen(port)) return true;
        Sleep(200);
    }
    return false;
}

// -----------------------------------------------------------------------------
// Python server
// -----------------------------------------------------------------------------
static bool TryLaunchPython(const wchar_t* pythonExe)
{
    const std::wstring script = g_editorDir + L"\\behavioru_server.py";
    std::wstring cmdLine = std::wstring(L"\"") + pythonExe + L"\" -u \""
                         + script + L"\" "
                         + std::to_wstring(kHttpPort)
                         + L" \"" + g_editorDir + L"\"";

    // Redirect stdout + stderr to a log file for diagnostics
    SECURITY_ATTRIBUTES sa{};
    sa.nLength              = sizeof(sa);
    sa.bInheritHandle       = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE hLog = CreateFileW(
        g_serverLogPath.c_str(),
        GENERIC_WRITE, FILE_SHARE_READ,
        &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hLog == INVALID_HANDLE_VALUE)
        hLog = nullptr;  // fall back to no capture rather than failing

    STARTUPINFOW si{};
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESHOWWINDOW | (hLog ? STARTF_USESTDHANDLES : 0);
    si.wShowWindow = SW_HIDE;
    if (hLog)
    {
        si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = hLog;
        si.hStdError  = hLog;
    }

    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessW(nullptr, cmdLine.data(),
                        nullptr, nullptr, hLog ? TRUE : FALSE,
                        CREATE_NO_WINDOW,
                        nullptr, g_editorDir.c_str(),
                        &si, &pi);
    if (hLog) CloseHandle(hLog);
    if (!ok) return false;

    CloseHandle(pi.hThread);
    g_pythonProcess = pi.hProcess;
    return true;
}

static bool StartPythonServer()
{
    const std::wstring script = g_editorDir + L"\\behavioru_server.py";
    if (!PathExists(script))
    {
        const std::wstring msg = L"Python script not found:\n" + script;
        MessageBoxW(g_hwnd, msg.c_str(), L"BehaviorULauncher", MB_ICONERROR);
        return false;
    }

    // Prefer absolute paths first to avoid py.exe launcher indirection
    // (py.exe spawns python.exe as a child and exits, which can confuse
    //  the process handle tracking and slow down the port-ready detection).
    static const wchar_t* absolutePaths[] = {
        L"C:\\Users\\Administrator\\AppData\\Local\\Programs\\Python\\Python311\\python.exe",
        L"C:\\Python313\\python.exe",
        L"C:\\Python312\\python.exe",
        L"C:\\Python311\\python.exe",
        L"C:\\Python310\\python.exe",
        L"C:\\Python39\\python.exe",
    };
    for (const auto* path : absolutePaths)
    {
        if (PathExists(path) && TryLaunchPython(path)) return true;
    }

    // Fall back to PATH-based launchers
    static const wchar_t* candidates[] = { L"python", L"python3", L"py" };
    for (const auto* exe : candidates)
    {
        if (TryLaunchPython(exe)) return true;
    }

    return false;
}

static void StopPythonServer()
{
    if (g_pythonProcess != INVALID_HANDLE_VALUE)
    {
        TerminateProcess(g_pythonProcess, 0);
        WaitForSingleObject(g_pythonProcess, 2000);
        CloseHandle(g_pythonProcess);
        g_pythonProcess = INVALID_HANDLE_VALUE;
    }
}

// -----------------------------------------------------------------------------
// Navigate the embedded (or fallback external) browser to the editor URL
// -----------------------------------------------------------------------------
static void OpenBrowserExternal()
{
    const HINSTANCE result = ShellExecuteW(
        g_hwnd, L"open", g_navigateUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32)
    {
        MessageBoxW(g_hwnd,
            L"Failed to open the browser.\n"
            L"Please open the following URL manually:\n\n"
            L"http://127.0.0.1:18102/index.html?host=127.0.0.1&port=17654",
            L"BehaviorULauncher", MB_ICONWARNING);
    }
}

static void NavigateEditor(const std::wstring& url)
{
#ifdef WEBVIEW2_ENABLED
    if (g_wv2Ready && g_wv2)
    {
        g_wv2->Navigate(url.c_str());
        return;
    }
    // WebView2 not yet ready - remember URL and navigate once it is
    g_pendingUrl = url;
#else
    OpenBrowserExternal();
#endif
}

// -----------------------------------------------------------------------------
// WebView2 initialisation  (called once after the window is shown)
// -----------------------------------------------------------------------------
#ifdef WEBVIEW2_ENABLED
static void InitWebView2(HWND hwnd, const std::wstring& exeDir)
{
    // Use a deterministic folder next to the launcher exe so the WebView2 user
    // data (cookies, cache, profile) lives with the binary regardless of the
    // editor folder layout. The relative path from g_editorDir was fragile and
    // depended on the binary living in a specific subfolder.
    const std::wstring userDataFolder = exeDir + L"\\WebView2UserData";

    // WebView2 requires the folder to exist; ignore "already exists" errors.
    if (!CreateDirectoryW(userDataFolder.c_str(), nullptr))
    {
        const DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS)
        {
            PostMessageW(hwnd, WM_WEBVIEW2_FAIL, 0, 0);
            return;
        }
    }

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userDataFolder.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd](HRESULT hr, ICoreWebView2Environment* env) -> HRESULT
            {
                if (FAILED(hr) || !env)
                {
                    PostMessageW(hwnd, WM_WEBVIEW2_FAIL, 0, 0);
                    return S_OK;
                }
                env->CreateCoreWebView2Controller(hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hwnd](HRESULT hr2, ICoreWebView2Controller* ctrl) -> HRESULT
                        {
                            if (FAILED(hr2) || !ctrl)
                            {
                                PostMessageW(hwnd, WM_WEBVIEW2_FAIL, 0, 0);
                                return S_OK;
                            }
                            ctrl->AddRef();
                            g_wv2Controller = ctrl;
                            ctrl->get_CoreWebView2(&g_wv2);

                            // Position WebView2 below the toolbar
                            RECT rc{};
                            GetClientRect(hwnd, &rc);
                            rc.top = kToolbarHeight;
                            ctrl->put_Bounds(rc);
                            ctrl->put_IsVisible(TRUE);

                            // Hide the default context menu and status bar for a cleaner look
                            ICoreWebView2Settings* settings = nullptr;
                            if (SUCCEEDED(g_wv2->get_Settings(&settings)) && settings)
                            {
                                settings->put_AreDefaultContextMenusEnabled(FALSE);
                                settings->put_IsStatusBarEnabled(FALSE);
                                settings->Release();
                            }

                            g_wv2Ready = true;
                            PostMessageW(hwnd, WM_WEBVIEW2_READY, 0, 0);
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());
}
#endif

// -----------------------------------------------------------------------------
// Win32 window procedure
// -----------------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        // Menu bar
        g_hMenu = CreateMenu();
        HMENU hFile = CreatePopupMenu();
        AppendMenuW(hFile, MF_STRING, ID_MENU_RESTART, L"&Restart Editor");
        AppendMenuW(hFile, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hFile, MF_STRING, ID_MENU_LOG, L"&View Log");
        AppendMenuW(g_hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hFile), L"&Actions");
        SetMenu(hwnd, g_hMenu);

        // Dark background
        SetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND,
            reinterpret_cast<LONG_PTR>(CreateSolidBrush(RGB(30, 30, 30))));

        // Poll timer
        SetTimer(hwnd, kTimerPoll, kPollIntervalMs, nullptr);
        return 0;
    }

    case WM_SERVER_READY:
        EnableMenuItem(g_hMenu, ID_MENU_RESTART, MF_BYCOMMAND | MF_ENABLED);
        DrawMenuBar(hwnd);
        NavigateEditor(g_navigateUrl);
        return 0;

    case WM_SERVER_SKIP:
        NavigateEditor(g_navigateUrl);
        return 0;

    case WM_TIMER:
        if (wp == kTimerPoll)
        {
            // keep polling; no status label to update
            (void)IsPortOpen(kHttpPort);
        }
        return 0;

#ifdef WEBVIEW2_ENABLED
    case WM_WEBVIEW2_READY:
        if (!g_pendingUrl.empty())
        {
            g_wv2->Navigate(g_pendingUrl.c_str());
            g_pendingUrl.clear();
        }
        return 0;

    case WM_WEBVIEW2_FAIL:
        if (!g_navigateUrl.empty())
            OpenBrowserExternal();
        return 0;

    case WM_SIZE:
        if (g_wv2Controller)
        {
            RECT rc{};
            GetClientRect(hwnd, &rc);
            rc.top = kToolbarHeight;
            g_wv2Controller->put_Bounds(rc);
        }
        return 0;
#endif

    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case ID_MENU_RESTART:
        {
            // Stop the running server and disable the menu item while restarting
            EnableMenuItem(g_hMenu, ID_MENU_RESTART, MF_BYCOMMAND | MF_GRAYED);
            DrawMenuBar(hwnd);
            StopPythonServer();
            if (StartPythonServer())
            {
                std::thread([hwnd]() {
                    const bool ok = WaitForPort(kHttpPort, 12000);
                    PostMessageW(hwnd, WM_SERVER_READY, ok ? 1 : 0, 0);
                }).detach();
            }
            else
            {
                EnableMenuItem(g_hMenu, ID_MENU_RESTART, MF_BYCOMMAND | MF_ENABLED);
                DrawMenuBar(hwnd);
            }
            break;
        }
        case ID_MENU_LOG:
            if (!g_serverLogPath.empty())
                ShellExecuteW(hwnd, L"open", g_serverLogPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            break;
        }
        return 0;

    case WM_CTLCOLORSTATIC:
    {
        const HDC hdc = reinterpret_cast<HDC>(wp);
        SetBkColor(hdc, RGB(30, 30, 30));
        SetTextColor(hdc, RGB(220, 220, 220));
        return reinterpret_cast<LRESULT>(CreateSolidBrush(RGB(30, 30, 30)));
    }

    case WM_DESTROY:
        KillTimer(hwnd, kTimerPoll);
        StopPythonServer();
#ifdef WEBVIEW2_ENABLED
        if (g_wv2)           { g_wv2->Release();           g_wv2           = nullptr; }
        if (g_wv2Controller) { g_wv2Controller->Release(); g_wv2Controller = nullptr; }
#endif
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// -----------------------------------------------------------------------------
// Entry point
// -----------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int)
{
    // Single-instance guard: create a named mutex; if it already exists another
    // instance is running — alert the user and exit immediately.
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"BehaviorULauncher_SingleInstance");
    if (hMutex == nullptr || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        MessageBoxW(nullptr,
            L"BehaviorULauncher is already running.\n\nOnly one instance may run at a time.",
            L"BehaviorULauncher", MB_ICONWARNING | MB_OK);
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    INITCOMMONCONTROLSEX icex{ sizeof(icex), ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icex);

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // -- Resolve paths ---------------------------------------------------------
    const std::wstring exeDir = GetExeDir();
    g_editorDir = ResolvePath(exeDir, L"..\\Editor");

    // Log file for Python server output (next to the exe for easy access)
    g_serverLogPath = exeDir + L"\\behavioru_server.log";

    if (!PathExists(g_editorDir))
    {
        const std::wstring msg =
            L"Editor folder not found:\n" + g_editorDir +
            L"\n\nExpected layout:\n"
            L"  <ProjectRoot>\\Plugins\\BehaviorUPlugin\\BehaviorULauncher\\BehaviorULauncher.exe\n"
            L"  <ProjectRoot>\\Plugins\\BehaviorUPlugin\\Editor\\";
        MessageBoxW(nullptr, msg.c_str(), L"BehaviorULauncher", MB_ICONERROR);
        return 1;
    }

    g_navigateUrl = L"http://127.0.0.1:"
                  + std::to_wstring(kHttpPort)
                  + L"/index.html?host=127.0.0.1&port="
                  + std::to_wstring(kWsPort);

    // -- Create the launcher window --------------------------------------------
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
    wc.lpszClassName = kWindowClass;
    wc.hIcon         = static_cast<HICON>(LoadImageW(
        hInst, MAKEINTRESOURCEW(IDI_LAUNCHER_ICON), IMAGE_ICON,
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
    wc.hIconSm       = static_cast<HICON>(LoadImageW(
        hInst, MAKEINTRESOURCEW(IDI_LAUNCHER_ICON), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    RegisterClassExW(&wc);

    g_hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        kWindowClass, kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 860,
        nullptr, nullptr, hInst, nullptr);
    // Menu is attached inside WM_CREATE via SetMenu

    if (!g_hwnd)
    {
        MessageBoxW(nullptr, L"Failed to create application window.", L"BehaviorULauncher", MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_hwnd, SW_SHOWNORMAL);
    UpdateWindow(g_hwnd);

#ifdef WEBVIEW2_ENABLED
    InitWebView2(g_hwnd, exeDir);
#endif

    // -- Python server (started on background thread to keep UI responsive) ----
    if (IsPortOpen(kHttpPort))
    {
        // Already up - just tell the window asynchronously so message loop runs first
        PostMessageW(g_hwnd, WM_SERVER_SKIP, 0, 0);
    }
    else
    {
        if (!StartPythonServer())
        {
            MessageBoxW(g_hwnd,
                L"Could not start the Python HTTP server.\n\n"
                L"Make sure Python is installed and 'py' or 'python' is in PATH.",
                L"BehaviorULauncher", MB_ICONERROR);
            DestroyWindow(g_hwnd);
            return 1;
        }

        // Wait for the server on a background thread; post result back to window
        std::thread([hwnd = g_hwnd]() {
            const bool ok = WaitForPort(kHttpPort, 12000);
            PostMessageW(hwnd, WM_SERVER_READY, ok ? 1 : 0, 0);
        }).detach();
    }

    // -- Message loop ----------------------------------------------------------
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    WSACleanup();
    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
