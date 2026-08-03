// AuraAbilityGraphLauncher.cpp
//
// Standalone Windows launcher for the AuraAbilityGraph Ability Editor.
//   1. Starts  Plugins/AuraAbilityGraph/Editor/ability_graph_server.py  on port 18103.
//   2. Waits for the server to respond (up to 8 s).
//   3. Opens the web app embedded using WebView2 (fallback: system browser).
//   4. Shows a compact status window that keeps the server alive.
//   5. On window close: terminates the Python process and all resources.
//
// Build:  run build.bat
// Output: Plugins/AuraAbilityGraph/AuraAbilityGraphLauncher/AuraAbilityGraphLauncher.exe

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
#include <unknwn.h>
#include "WebView2.h"
#include <wrl.h>
using namespace Microsoft::WRL;
#endif

// -----------------------------------------------------------------------------
// Constants
// -----------------------------------------------------------------------------
static constexpr int    kHttpPort      = 18103;
static constexpr UINT   kTimerPoll     = 1;
static constexpr UINT   kPollIntervalMs = 3000;

static constexpr UINT   WM_SERVER_READY   = WM_APP + 1;
static constexpr UINT   WM_SERVER_SKIP    = WM_APP + 2;
static constexpr UINT   WM_WEBVIEW2_READY = WM_APP + 3;
static constexpr UINT   WM_WEBVIEW2_FAIL  = WM_APP + 4;

static constexpr int    kToolbarHeight    = 0;

static const WCHAR kWindowClass[] = L"AuraAbilityGraphLauncherWnd";
static const WCHAR kWindowTitle[] = L"AuraAbilityGraph - Ability Graph Editor";

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
static std::wstring             g_pendingUrl;
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
static bool TryLaunchPython(const std::wstring& pythonExe)
{
    const std::wstring script = g_editorDir + L"\\ability_graph_server.py";
    std::wstring cmdLine = std::wstring(L"\"") + pythonExe + L"\" -u \""
                         + script + L"\" "
                         + std::to_wstring(kHttpPort)
                         + L" \"" + g_editorDir + L"\"";

    SECURITY_ATTRIBUTES sa{};
    sa.nLength              = sizeof(sa);
    sa.bInheritHandle       = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE hLog = CreateFileW(
        g_serverLogPath.c_str(),
        GENERIC_WRITE, FILE_SHARE_READ,
        &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hLog == INVALID_HANDLE_VALUE)
        hLog = nullptr;

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
    const std::wstring script = g_editorDir + L"\\ability_graph_server.py";
    if (!PathExists(script))
    {
        const std::wstring msg = L"Python script not found:\n" + script;
        MessageBoxW(g_hwnd, msg.c_str(), L"AuraAbilityGraphLauncher", MB_ICONERROR);
        return false;
    }

    // Resolve Python through the user's PATH so the launcher is portable across
    // machines and Python installations.  SearchPathW also handles the Windows
    // Python launcher (py.exe) without embedding a developer-specific path.
    static const wchar_t* candidates[] = { L"python.exe", L"python3.exe", L"py.exe" };
    for (const auto* exe : candidates)
    {
        WCHAR resolvedPath[MAX_PATH] = {};
        const DWORD ResolvedLength = SearchPathW(nullptr, exe, nullptr, MAX_PATH, resolvedPath, nullptr);
        if (ResolvedLength > 0 && ResolvedLength < MAX_PATH && TryLaunchPython(resolvedPath))
        {
            return true;
        }
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
// Browser navigation
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
            L"http://127.0.0.1:18103/index.html",
            L"AuraAbilityGraphLauncher", MB_ICONWARNING);
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
    g_pendingUrl = url;
#else
    OpenBrowserExternal();
#endif
}

// -----------------------------------------------------------------------------
// WebView2 initialization
// -----------------------------------------------------------------------------
#ifdef WEBVIEW2_ENABLED
static void InitWebView2(HWND hwnd, const std::wstring& exeDir)
{
    const std::wstring userDataFolder = exeDir + L"\\WebView2UserData";

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

                            RECT rc{};
                            GetClientRect(hwnd, &rc);
                            rc.top = kToolbarHeight;
                            ctrl->put_Bounds(rc);
                            ctrl->put_IsVisible(TRUE);

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
        g_hMenu = CreateMenu();
        HMENU hFile = CreatePopupMenu();
        AppendMenuW(hFile, MF_STRING, ID_MENU_RESTART, L"&Restart Editor");
        AppendMenuW(hFile, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hFile, MF_STRING, ID_MENU_LOG, L"&View Log");
        AppendMenuW(g_hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hFile), L"&Actions");
        SetMenu(hwnd, g_hMenu);

        SetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND,
            reinterpret_cast<LONG_PTR>(CreateSolidBrush(RGB(30, 30, 30))));

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
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"AuraAbilityGraphLauncher_SingleInstance");
    if (hMutex == nullptr || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        MessageBoxW(nullptr,
            L"AuraAbilityGraphLauncher is already running.\n\nOnly one instance may run at a time.",
            L"AuraAbilityGraphLauncher", MB_ICONWARNING | MB_OK);
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    INITCOMMONCONTROLSEX icex{ sizeof(icex), ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icex);

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    const std::wstring exeDir = GetExeDir();
    g_editorDir = ResolvePath(exeDir, L"..\\Editor");

    g_serverLogPath = exeDir + L"\\ability_graph_server.log";

    if (!PathExists(g_editorDir))
    {
        const std::wstring msg =
            L"Editor folder not found:\n" + g_editorDir +
            L"\n\nExpected layout:\n"
            L"  <ProjectRoot>\\Plugins\\AuraAbilityGraph\\AuraAbilityGraphLauncher\\AuraAbilityGraphLauncher.exe\n"
            L"  <ProjectRoot>\\Plugins\\AuraAbilityGraph\\Editor\\";
        MessageBoxW(nullptr, msg.c_str(), L"AuraAbilityGraphLauncher", MB_ICONERROR);
        return 1;
    }

    g_navigateUrl = L"http://127.0.0.1:"
                  + std::to_wstring(kHttpPort)
                  + L"/index.html";

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
    wc.lpszClassName = kWindowClass;
    RegisterClassExW(&wc);

    g_hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        kWindowClass, kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 860,
        nullptr, nullptr, hInst, nullptr);

    if (!g_hwnd)
    {
        MessageBoxW(nullptr, L"Failed to create application window.", L"AuraAbilityGraphLauncher", MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_hwnd, SW_SHOWNORMAL);
    UpdateWindow(g_hwnd);

#ifdef WEBVIEW2_ENABLED
    InitWebView2(g_hwnd, exeDir);
#endif

    if (IsPortOpen(kHttpPort))
    {
        PostMessageW(g_hwnd, WM_SERVER_SKIP, 0, 0);
    }
    else
    {
        if (!StartPythonServer())
        {
            MessageBoxW(g_hwnd,
                L"Could not start the Python HTTP server.\n\n"
                L"Make sure Python is installed and 'py' or 'python' is in PATH.",
                L"AuraAbilityGraphLauncher", MB_ICONERROR);
            DestroyWindow(g_hwnd);
            return 1;
        }

        std::thread([hwnd = g_hwnd]() {
            const bool ok = WaitForPort(kHttpPort, 12000);
            PostMessageW(hwnd, WM_SERVER_READY, ok ? 1 : 0, 0);
        }).detach();
    }

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
