#include "NativeWindow.h"
#include <windows.h>
#include <wrl.h>
#include <WebView2.h>
#include <chrono>
#include <iostream>
#include <stdexcept>
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

struct NativeWindow::State {
    HWND window = nullptr;
    bool closing = false, loaded = false;
    std::string error;
    std::wstring url;
    std::chrono::steady_clock::time_point started;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> view;

    void fail(const char* stage, HRESULT hr) {
        if (closing || !error.empty()) return;
        char code[32]{}; sprintf_s(code, "0x%08lX", static_cast<unsigned long>(hr));
        error = std::string(stage) + " (" + code + "). Install or repair Microsoft Edge WebView2 Runtime. "
            "For unattended operation, launch AuraGSM with --headless.";
    }
    void resize() {
        if (controller && window) { RECT bounds{}; GetClientRect(window, &bounds); controller->put_Bounds(bounds); }
    }
    static LRESULT CALLBACK procedure(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
        auto* s = reinterpret_cast<State*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            s = static_cast<State*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
            s->window = hwnd; SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(s));
        }
        if (s) switch (message) {
            case WM_SIZE: s->resize(); return 0;
            case WM_MOVE: if (s->controller) s->controller->NotifyParentWindowPositionChanged(); break;
            case WM_DPICHANGED: {
                auto* rect = reinterpret_cast<RECT*>(lp);
                SetWindowPos(hwnd, nullptr, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
                s->resize(); return 0;
            }
            case WM_SETFOCUS: if (s->controller) s->controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC); return 0;
            case WM_GETMINMAXINFO: {
                auto* info = reinterpret_cast<MINMAXINFO*>(lp); info->ptMinTrackSize = {480, 480}; return 0;
            }
            case WM_CLOSE: s->closing = true; DestroyWindow(hwnd); return 0;
            // The GSM loop exits through closing. Posting WM_QUIT here would also
            // dismiss the error dialog opened after an initialization failure.
            case WM_DESTROY: s->closing = true; s->window = nullptr; return 0;
            case WM_PAINT: {
                PAINTSTRUCT paint{}; HDC dc = BeginPaint(hwnd, &paint); RECT bounds{}; GetClientRect(hwnd, &bounds);
                SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(220, 235, 240));
                DrawTextW(dc, L"Loading the GSM dashboard...", -1, &bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                EndPaint(hwnd, &paint); return 0;
            }
        }
        return DefWindowProcW(hwnd, message, wp, lp);
    }
};

NativeWindow::NativeWindow() : state(std::make_shared<State>()) {}
NativeWindow::~NativeWindow() {
    // Pending creation callbacks retain State, but never touch a destroyed HWND.
    state->closing = true;
    if (state->controller) state->controller->Close();
    state->view.Reset(); state->controller.Reset();
    if (state->window) DestroyWindow(state->window);
    if (comInitialized) CoUninitialize();
}

void NativeWindow::open(int port, const std::filesystem::path& profileDirectory) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) throw std::runtime_error("Cannot initialize the WebView2 STA thread");
    comInitialized = true;
    // Configure DPI before creating this process's first native window.
    using SetDpiContext = BOOL(WINAPI*)(HANDLE);
    auto setDpi = reinterpret_cast<SetDpiContext>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext"));
    if (setDpi) setDpi(reinterpret_cast<HANDLE>(-4));
    const wchar_t* className = L"AuraGSM.NativeDashboard";
    WNDCLASSW wc{}; wc.lpfnWndProc = State::procedure; wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = className; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) throw std::runtime_error("Cannot register GSM window");
    state->url = L"http://127.0.0.1:" + std::to_wstring(port) + L"/";
    state->started = std::chrono::steady_clock::now();
    std::filesystem::create_directories(profileDirectory);
    HWND hwnd = CreateWindowExW(0, className, L"Aura GSM | Dedicated Server Manager", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1380, 900, nullptr, nullptr, wc.hInstance, state.get());
    if (!hwnd) throw std::runtime_error("Cannot create GSM window");
    ShowWindow(hwnd, SW_SHOW); UpdateWindow(hwnd);
    auto s = state;
    hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, profileDirectory.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [s](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                if (s->closing) return S_OK;
                if (FAILED(result) || !environment) { s->fail("WebView2 environment creation failed", result); return S_OK; }
                HRESULT create = environment->CreateCoreWebView2Controller(s->window,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [s](HRESULT status, ICoreWebView2Controller* controller) -> HRESULT {
                            if (s->closing) { if (controller) controller->Close(); return S_OK; }
                            if (FAILED(status) || !controller) { s->fail("WebView2 controller creation failed", status); return S_OK; }
                            s->controller = controller;
                            HRESULT getView = controller->get_CoreWebView2(&s->view);
                            if (FAILED(getView) || !s->view) { s->fail("WebView2 initialization failed", getView); return S_OK; }
                            ComPtr<ICoreWebView2Settings> settings;
                            if (SUCCEEDED(s->view->get_Settings(&settings))) {
                                settings->put_AreDevToolsEnabled(FALSE); settings->put_AreDefaultContextMenusEnabled(FALSE);
                                settings->put_IsStatusBarEnabled(FALSE); settings->put_IsWebMessageEnabled(FALSE);
                                settings->put_AreHostObjectsAllowed(FALSE);
                            }
                            // Only the exact local dashboard document can navigate in this host.
                            // No shell/browser handoff, native bridge, new windows or permission grants.
                            std::weak_ptr<State> weak = s; EventRegistrationToken token{};
                            s->view->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
                                [weak](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                                    auto current = weak.lock(); LPWSTR uri = nullptr; args->get_Uri(&uri);
                                    if (!current || !uri || (current->url != uri && current->url + L"index.html" != uri)) args->put_Cancel(TRUE);
                                    CoTaskMemFree(uri); return S_OK;
                                }).Get(), &token);
                            s->view->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                                [](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT { args->put_Handled(TRUE); return S_OK; }).Get(), &token);
                            s->view->add_PermissionRequested(Callback<ICoreWebView2PermissionRequestedEventHandler>(
                                [](ICoreWebView2*, ICoreWebView2PermissionRequestedEventArgs* args) -> HRESULT { args->put_State(COREWEBVIEW2_PERMISSION_STATE_DENY); return S_OK; }).Get(), &token);
                            s->view->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                [weak](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                                    if (auto current = weak.lock()) {
                                        BOOL success = FALSE; args->get_IsSuccess(&success);
                                        if (success) { current->loaded = true; std::cout << "Native WebView2 dashboard loaded" << std::endl; }
                                        else if (!current->loaded) current->fail("Dashboard navigation failed", E_FAIL);
                                    }
                                    return S_OK;
                                }).Get(), &token);
                            s->view->add_ProcessFailed(Callback<ICoreWebView2ProcessFailedEventHandler>(
                                [weak](ICoreWebView2*, ICoreWebView2ProcessFailedEventArgs*) -> HRESULT {
                                    if (auto current = weak.lock()) current->fail("WebView2 browser process failed", E_FAIL); return S_OK;
                                }).Get(), &token);
                            s->resize(); controller->put_IsVisible(TRUE);
                            HRESULT nav = s->view->Navigate(s->url.c_str()); if (FAILED(nav)) s->fail("Cannot navigate to GSM dashboard", nav);
                            return S_OK;
                        }).Get());
                if (FAILED(create)) s->fail("Cannot create WebView2 controller", create);
                return S_OK;
            }).Get());
    if (FAILED(hr)) state->fail("Cannot start WebView2; runtime may be missing", hr);
}

bool NativeWindow::pump() {
    MSG message{};
    // Bound message processing so a busy WebView cannot starve DS deadlines.
    for (int i = 0; i < 64 && PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE); ++i) {
        if (message.message == WM_QUIT) state->closing = true;
        TranslateMessage(&message); DispatchMessageW(&message);
    }
    if (!state->error.empty()) throw std::runtime_error(state->error);
    if (!state->closing && !state->loaded && std::chrono::steady_clock::now() - state->started > std::chrono::seconds(30))
        throw std::runtime_error("WebView2 dashboard initialization exceeded 30 seconds. Check the runtime, or use --headless.");
    return !state->closing;
}
