#pragma once
#include <filesystem>
#include <memory>
#include <string>

// STA window and WebView2 callbacks live on the same thread as the GSM event loop.
class NativeWindow {
public:
    NativeWindow();
    ~NativeWindow();
    NativeWindow(const NativeWindow&) = delete;
    NativeWindow& operator=(const NativeWindow&) = delete;
    void open(int webPort, const std::filesystem::path& profileDirectory);
    bool pump();
private:
    struct State;
    std::shared_ptr<State> state;
    bool comInitialized = false;
};
