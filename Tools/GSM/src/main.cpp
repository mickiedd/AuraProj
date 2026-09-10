// Standalone Windows GSM. All mutable process/connection state belongs to the event loop.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include <psapi.h>
#include <shellapi.h>
#include "NativeWindow.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <vector>
using Json = nlohmann::json;
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
static double now() { return std::chrono::duration<double>(Clock::now().time_since_epoch()).count(); }
static volatile LONG stopping = 0;
static BOOL WINAPI onSignal(DWORD) { InterlockedExchange(&stopping, 1); return TRUE; }
static std::string lower(std::string s) { for (auto& c : s) c = static_cast<char>(tolower(static_cast<unsigned char>(c))); return s; }
static std::wstring wide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (!n) throw std::runtime_error("Invalid UTF-8");
    std::wstring w(n, 0); MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n); return w;
}
static std::string utf8(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0); WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr); return s;
}
static std::string env(const wchar_t* key) {
    DWORD n = GetEnvironmentVariableW(key, nullptr, 0); if (!n) return {};
    std::wstring w(n, 0); w.resize(GetEnvironmentVariableW(key, w.data(), n)); return utf8(w);
}
static std::string readFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary); if (!f) throw std::runtime_error("Cannot read " + p.u8string());
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
static Json error(const std::string& s) { return {{"status", "error"}, {"message", s}}; }
static bool equalSecret(const std::string& a, const std::string& b) {
    size_t diff = a.size() ^ b.size();
    for (size_t i = 0; i < a.size(); ++i) diff |= static_cast<unsigned char>(a[i]) ^ (i < b.size() ? static_cast<unsigned char>(b[i]) : 0);
    return diff == 0;
}
static std::string nonce() {
    unsigned char b[32]; if (BCryptGenRandom(nullptr, b, sizeof(b), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) throw std::runtime_error("Random generator failed");
    const char* hex = "0123456789abcdef"; std::string s; for (auto c : b) { s += hex[c >> 4]; s += hex[c & 15]; } return s;
}
// CommandLineToArgv/CRT quoting: double backslashes before quotes and the closing quote.
static std::wstring quote(const std::wstring& s) {
    std::wstring out = L"\""; size_t slashes = 0;
    for (wchar_t c : s) {
        if (c == L'\\') { ++slashes; continue; }
        out.append(slashes * (c == L'"' ? 2 : 1), L'\\'); slashes = 0;
        if (c == L'"') out += L'\\'; out += c;
    }
    out.append(slashes * 2, L'\\'); return out + L'"';
}
static bool editor(const fs::path& p) {
    const auto n = lower(p.filename().u8string());
    static const std::set<std::string> names = {"unrealeditor.exe", "unrealeditor-cmd.exe", "unrealeditor-win64-debuggame.exe", "unrealeditor-win64-development.exe", "unrealeditor-win64-shipping.exe", "unrealeditor-win64-test.exe"};
    return names.count(n) != 0;
}
// Requests select a known variant, never an arbitrary executable path.
static fs::path normalizedEngine(fs::path p) {
    p = fs::weakly_canonical(p);
    while (!p.empty() && p.filename().empty() && p != p.root_path()) p = p.parent_path();
    if (lower(p.filename().u8string()) != "engine") p /= "Engine";
    return fs::weakly_canonical(p);
}
static std::wstring registryString(HKEY hive, const std::wstring& key, const std::wstring& name) {
    DWORD bytes = 0;
    if (RegGetValueW(hive, key.c_str(), name.c_str(), RRF_RT_REG_SZ, nullptr, nullptr, &bytes) != ERROR_SUCCESS) return {};
    std::wstring value(bytes / sizeof(wchar_t), 0);
    if (RegGetValueW(hive, key.c_str(), name.c_str(), RRF_RT_REG_SZ, nullptr, value.data(), &bytes) != ERROR_SUCCESS) return {};
    while (!value.empty() && value.back() == 0) value.pop_back();
    return value;
}
static std::vector<fs::path> projectEngines(const fs::path& root) {
    std::vector<fs::path> engines;
    const auto project = root / "Aura.uproject";
    if (!fs::is_regular_file(project)) return engines;
    const auto association = Json::parse(readFile(project)).value("EngineAssociation", "");
    if (association.empty() || association.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-{}") != std::string::npos) return engines;
    const auto registered = registryString(HKEY_CURRENT_USER, LR"(Software\Epic Games\Unreal Engine\Builds)", wide(association));
    const auto installed = registryString(HKEY_LOCAL_MACHINE, LR"(SOFTWARE\EpicGames\Unreal Engine\)" + wide(association), L"InstalledDirectory");
    for (const auto& candidate : {root.parent_path() / fs::u8path("UE_" + association), fs::path(registered), fs::path(installed)}) {
        if (!candidate.empty() && fs::is_directory(candidate / "Engine/Binaries/Win64")) engines.push_back(normalizedEngine(candidate));
    }
    return engines;
}
static int portNumber(const std::string& s) {
    size_t used = 0; int n = std::stoi(s, &used);
    if (used != s.size() || n < 1 || n > 65535) throw std::runtime_error("Invalid port"); return n;
}
struct Level {
    Json config; HANDLE process = nullptr, job = nullptr; DWORD pid = 0;
    std::string state = "stopped", secret, failure, executable, log;
    std::vector<std::string> args; double started = 0, ended = 0; int launches = 0;
    Json exitCode = nullptr;
    ~Level() { cleanup(); }
    void cleanup() { if (job) CloseHandle(job); job = nullptr; if (process) CloseHandle(process); process = nullptr; secret.clear(); }
    bool alive() const { return process && WaitForSingleObject(process, 0) == WAIT_TIMEOUT; }
};
struct Connection {
    SOCKET socket = INVALID_SOCKET; bool http = false; std::string input, output, waiting; size_t sent = 0;
    double accepted = now(), writeStarted = 0;
    ~Connection() { if (socket != INVALID_SOCKET) closesocket(socket); }
    void reply(const Json& j) { output = j.dump() + "\n"; waiting.clear(); writeStarted = now(); }
};
class Manager {
    fs::path root, web, server, editorExe;
    int tcpPort = 9000, httpPort = 9080;
    std::string publicHost = "127.0.0.1", provider = "NULL", auth, serverAuth;
    std::map<std::string, std::unique_ptr<Level>> levels;
    std::map<std::string, std::string> assets;
    std::vector<std::unique_ptr<Connection>> clients;
    SOCKET tcp = INVALID_SOCKET, http = INVALID_SOCKET;
    double boot = now(); unsigned long long requests = 0;
    std::string redact(std::string s) const {
        for (const auto& secret : {auth, serverAuth}) if (!secret.empty()) {
            size_t p = 0; while ((p = s.find(secret, p)) != std::string::npos) { s.replace(p, secret.size(), "[redacted]"); p += 10; }
        }
        return s;
    }
    std::vector<std::string> safeArgs(const std::vector<std::string>& args) const {
        auto result = args; bool hideNext = false;
        for (auto& a : result) {
            if (hideNext) { a = "[redacted]"; hideNext = false; continue; }
            const auto key = lower(a.substr(0, a.find('=')));
            if (key.find("token") != std::string::npos || key.find("password") != std::string::npos || key.find("secret") != std::string::npos || key.find("nonce") != std::string::npos) {
                auto p = a.find('='); if (p == std::string::npos) hideNext = true; else a = a.substr(0, p + 1) + "[redacted]";
            }
            a = redact(a);
        }
        return result;
    }
    static SOCKET listenOn(int port) {
        SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); if (s == INVALID_SOCKET) throw std::runtime_error("Socket creation failed");
        BOOL exclusive = TRUE; setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<char*>(&exclusive), sizeof(exclusive));
        sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(static_cast<u_short>(port)); inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) || listen(s, SOMAXCONN)) { closesocket(s); throw std::runtime_error("Cannot listen on 127.0.0.1:" + std::to_string(port) + "; port may already be in use"); }
        u_long nonblocking = 1; ioctlsocket(s, FIONBIO, &nonblocking); return s;
    }
    fs::path chooseExecutable(const Json& r) {
        const fs::path client = fs::u8path(r.value("clientExecutable", ""));
        if (editor(client)) {
            const auto engine = r.value("clientEngineRoot", "");
            fs::path selected = editorExe;
            if (selected.empty()) {
                for (const auto& known : projectEngines(root)) {
                    if (!engine.empty() && lower(known.u8string()) != lower(normalizedEngine(fs::u8path(engine)).u8string())) continue;
                    const auto candidate = known / "Binaries/Win64" / client.filename();
                    if (fs::is_regular_file(candidate)) {
                        if (!selected.empty() && lower(selected.u8string()) != lower(candidate.u8string()))
                            throw std::runtime_error("Multiple project editors found; launch GSM from the intended Unreal Editor");
                        selected = candidate;
                    }
                }
                // Optional legacy fallback cannot override successful project discovery.
                if (selected.empty()) selected = fs::u8path(env(L"UE_EDITOR_EXE"));
            }
            if (selected.empty() || !fs::is_regular_file(selected) || lower(selected.filename().u8string()) != lower(client.filename().u8string()))
                throw std::runtime_error("Matching project editor was not found; launch GSM from the Unreal Editor menu");
            if (!engine.empty() && lower(normalizedEngine(selected.parent_path().parent_path().parent_path()).u8string()) != lower(normalizedEngine(fs::u8path(engine)).u8string()))
                throw std::runtime_error("Client engine differs from the project editor installation");
            return selected;
        }
        if (!server.empty() && fs::is_regular_file(server)) return server;
        if (server.empty()) {
            const auto packaged = root / "Saved/StagedBuilds/WindowsServer/AuraServer.exe";
            bool cooked = false;
            for (const auto& content : {packaged.parent_path() / "Aura/Content", packaged.parent_path() / "Content"}) {
                if (fs::is_regular_file(content / "Maps/StartupMap.umap")) cooked = true;
                if (fs::is_directory(content / "Paks")) for (const auto& file : fs::directory_iterator(content / "Paks"))
                    if (file.path().extension() == ".pak" || file.path().extension() == ".utoc") cooked = true;
            }
            if (fs::is_regular_file(packaged) && cooked) return packaged;
            if (!editorExe.empty() && fs::is_regular_file(editorExe)) return editorExe;
            const auto legacyEditor = fs::u8path(env(L"UE_EDITOR_EXE"));
            if (!legacyEditor.empty() && fs::is_regular_file(legacyEditor)) return legacyEditor;
        }
        throw std::runtime_error("Server executable unavailable; stage a cooked server or configure --server-exe / UE_EDITOR_EXE");
    }
    void launch(Level& l, const fs::path& exe) {
        // Current Unreal callback reads this file, not AURA_GSM_PORT. Fail explicitly
        // instead of launching a child that reports readiness to another manager.
        auto connectionConfig = root / "Content/Config/ServerConnection.json";
        if ((editor(exe) || lower(exe.filename().u8string()).find("auraserver") == 0) && fs::is_regular_file(connectionConfig)) {
            if (Json::parse(readFile(connectionConfig)).value("gameServerPort", 9000) != tcpPort)
                throw std::runtime_error("GSM port must match Content/Config/ServerConnection.json for Unreal readiness callbacks");
        }
        l.cleanup(); l.state = "failed"; l.failure.clear(); l.exitCode = nullptr; l.started = now(); l.ended = 0;
        l.executable = fs::absolute(exe).u8string();
        l.log = (root / "Saved/Logs/GameServerManagerNative" / (l.config.at("id").get<std::string>() + ".log")).u8string();
        fs::create_directories(fs::u8path(l.log).parent_path());
        l.args = {l.executable}; if (editor(exe)) l.args.push_back((root / "Aura.uproject").u8string());
        l.args.push_back(l.config.at("mapPath").get<std::string>() + "?port=" + std::to_string(l.config.at("port").get<int>()));
        for (auto& arg : l.config.at("launchArgs")) l.args.push_back(arg.get<std::string>());
        l.args.push_back("-port=" + std::to_string(l.config.at("port").get<int>()));
        if (l.config.value("queryPort", 0)) l.args.push_back("-QueryPort=" + std::to_string(l.config.at("queryPort").get<int>()));
        l.args.push_back("-Abslog=" + l.log); if (!provider.empty()) l.args.push_back("-AuraPersistenceProvider=" + provider);
        std::wstring cmd; for (const auto& a : l.args) { if (!cmd.empty()) cmd += L' '; cmd += quote(wide(a)); }
        l.secret = nonce();
        // Copy environment without mutating the manager's environment or leaking nonce in argv.
        std::map<std::wstring, std::wstring> overrides = {{L"AURA_GSM_SERVER_READY_NONCE", wide(l.secret)}, {L"AURA_GSM_ADDRESS", L"127.0.0.1"}, {L"AURA_GSM_PORT", std::to_wstring(tcpPort)}, {L"AURA_GSM_SERVER_AUTH_TOKEN", wide(serverAuth)}};
        std::vector<wchar_t> block; std::vector<std::wstring> environment; LPWCH original = GetEnvironmentStringsW();
        if (!original) throw std::runtime_error("Cannot read environment");
        for (auto p = original; *p; p += wcslen(p) + 1) {
            std::wstring item(p); auto split = item.find(L'='); bool replaced = false;
            for (auto& kv : overrides) if (_wcsicmp(item.substr(0, split).c_str(), kv.first.c_str()) == 0) replaced = true;
            if (!replaced) environment.push_back(item);
        }
        FreeEnvironmentStringsW(original);
        for (auto& kv : overrides) environment.push_back(kv.first + L"=" + kv.second);
        std::sort(environment.begin(), environment.end(), [](const auto& a, const auto& b) { return _wcsicmp(a.c_str(), b.c_str()) < 0; });
        for (const auto& item : environment) { block.insert(block.end(), item.begin(), item.end()); block.push_back(0); } block.push_back(0);
        l.job = CreateJobObjectW(nullptr, nullptr); JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!l.job || !SetInformationJobObject(l.job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) { l.cleanup(); throw std::runtime_error("Cannot create process ownership job"); }
        STARTUPINFOW si{}; si.cb = sizeof(si); PROCESS_INFORMATION pi{};
        if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW, block.data(), root.c_str(), &si, &pi)) {
            auto code = GetLastError(); l.cleanup(); throw std::runtime_error("CreateProcess failed, Windows error " + std::to_string(code));
        }
        if (!AssignProcessToJobObject(l.job, pi.hProcess)) {
            TerminateProcess(pi.hProcess, 1); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); l.cleanup(); throw std::runtime_error("Cannot assign child to ownership job");
        }
        l.process = pi.hProcess; l.pid = pi.dwProcessId;
        if (ResumeThread(pi.hThread) == static_cast<DWORD>(-1)) { CloseHandle(pi.hThread); l.cleanup(); throw std::runtime_error("Cannot resume child"); }
        CloseHandle(pi.hThread); l.state = "starting"; ++l.launches;
        std::cout << "Started level " << l.config.at("id").get<std::string>() << " pid=" << l.pid << std::endl;
    }
    void handleProtocol(Connection& c) {
        ++requests;
        try {
            auto r = Json::parse(c.input.substr(0, c.input.find('\n')));
            if (!r.is_object()) throw std::runtime_error("Request must be an object");
            const auto action = r.value("action", ""); const auto expected = action == "server_ready" ? serverAuth : auth;
            if (!expected.empty() && !equalSecret(expected, r.value(action == "server_ready" ? "serverAuthToken" : "authToken", ""))) { c.reply(error("Unauthorized control-plane request")); return; }
            if (action != "request_server" && action != "server_ready") { c.reply(error("Unknown action")); return; }
            const auto id = r.value("levelId", ""); auto found = levels.find(id);
            if (found == levels.end()) { c.reply(error("Unknown levelId")); return; }
            auto& l = *found->second;
            if (action == "server_ready") {
                if (!l.alive() || (l.state != "starting" && l.state != "ready") || !r.contains("port") || !r.at("port").is_number_integer() || r.at("port") != l.config.at("port") || l.secret.empty() || !equalSecret(l.secret, r.value("readyNonce", ""))) { c.reply(error("Invalid readiness process, port or nonce")); return; }
                l.state = "ready"; c.reply({{"status", "ok"}, {"message", "registered"}}); return;
            }
            if (std::count_if(clients.begin(), clients.end(), [](const auto& client) { return !client->waiting.empty(); }) >= 48) {
                c.reply(error("Too many pending server requests; retry shortly")); return;
            }
            const auto exe = fs::absolute(chooseExecutable(r));
            if (l.alive() && lower(l.executable) != lower(exe.u8string())) { c.reply(error("Level is running with a different runtime; stop the manager before switching")); return; }
            if (!l.alive()) {
                try { launch(l, exe); } catch (const std::exception& e) { l.failure = e.what(); l.state = "failed"; l.ended = now(); throw; }
            }
            c.waiting = id;
        } catch (const Json::exception&) { c.reply(error("Malformed JSON or invalid field type")); }
        catch (const std::exception& e) { c.reply(error(redact(e.what()))); }
    }
    void refresh() {
        for (auto& kv : levels) {
            auto& l = *kv.second;
            if (l.process && !l.alive()) {
                DWORD exit = 0; GetExitCodeProcess(l.process, &exit); l.exitCode = exit;
                l.failure = l.state == "starting" ? "Process exited before world readiness" : "Process exited";
                l.state = "exited"; l.ended = now(); l.cleanup();
            } else if (l.state == "starting" && now() - l.started >= 30) {
                l.failure = "World readiness exceeded 30-second launch deadline"; l.state = "failed"; l.ended = now(); l.cleanup();
            }
        }
        for (auto& c : clients) if (!c->waiting.empty()) {
            auto& l = *levels.at(c->waiting);
            if (l.state == "ready" && l.alive()) c->reply({{"status", "ready"}, {"host", publicHost}, {"port", l.config.at("port")}, {"serverMode", editor(fs::u8path(l.executable)) ? "editor" : "packaged"}});
            else if (l.state != "starting") c->reply(error(l.failure.empty() ? "Server unavailable" : l.failure));
        }
    }
    Json snapshot() {
        Json rows = Json::array(); int running = 0, ready = 0, starting = 0, failed = 0;
        for (auto& kv : levels) {
            auto& l = *kv.second; bool alive = l.alive(); running += alive; ready += alive && l.state == "ready"; starting += alive && l.state == "starting"; failed += l.state == "failed" || l.state == "exited";
            Json memory = nullptr, privateMemory = nullptr, cpu = nullptr;
            if (alive) {
                PROCESS_MEMORY_COUNTERS_EX m{}; m.cb = sizeof(m);
                if (GetProcessMemoryInfo(l.process, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&m), sizeof(m))) { memory = m.WorkingSetSize; privateMemory = m.PrivateUsage; }
                FILETIME a{}, b{}, k{}, u{};
                if (GetProcessTimes(l.process, &a, &b, &k, &u)) {
                    ULARGE_INTEGER kernel{}, user{}; kernel.LowPart = k.dwLowDateTime; kernel.HighPart = k.dwHighDateTime; user.LowPart = u.dwLowDateTime; user.HighPart = u.dwHighDateTime;
                    cpu = (kernel.QuadPart + user.QuadPart) / 10000000.0;
                }
            }
            rows.push_back({{"id", kv.first}, {"name", l.config.value("displayName", kv.first)}, {"map", l.config.at("mapPath")}, {"port", l.config.at("port")}, {"queryPort", l.config.value("queryPort", 0)}, {"state", l.state}, {"running", alive}, {"pid", l.pid ? Json(l.pid) : Json(nullptr)}, {"executable", redact(l.executable)}, {"arguments", safeArgs(l.args)}, {"configuredArguments", safeArgs(l.config.at("launchArgs").get<std::vector<std::string>>())}, {"logPath", l.log}, {"uptimeSeconds", l.started ? (alive ? now() : l.ended) - l.started : 0}, {"workingSetBytes", memory}, {"privateBytes", privateMemory}, {"cpuSeconds", cpu}, {"exitCode", l.exitCode}, {"launchCount", l.launches}, {"error", redact(l.failure)}});
        }
        return {{"version", "1.0.0"}, {"scope", "Processes owned by this GSM instance only"}, {"managerPid", GetCurrentProcessId()}, {"uptimeSeconds", now() - boot}, {"tcpEndpoint", "127.0.0.1:" + std::to_string(tcpPort)}, {"webEndpoint", "127.0.0.1:" + std::to_string(httpPort)}, {"publicHost", publicHost}, {"requestCount", requests}, {"counts", {{"configured", levels.size()}, {"running", running}, {"ready", ready}, {"starting", starting}, {"failed", failed}}}, {"levels", rows}};
    }
    void httpReply(Connection& c, int code, const std::string& type, const std::string& body) {
        c.output = "HTTP/1.1 " + std::to_string(code) + (code == 200 ? " OK" : " Error") + "\r\nContent-Type: " + type + "\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nContent-Security-Policy: default-src 'self'; frame-ancestors 'none'; object-src 'none'; base-uri 'none'\r\n\r\n" + body; c.writeStarted = now();
    }
    void handleHttp(Connection& c) {
        std::istringstream stream(c.input); std::string line, method, path, version;
        std::getline(stream, line); std::istringstream first(line); first >> method >> path >> version;
        std::string host; int hosts = 0;
        while (std::getline(stream, line) && line != "\r") {
            auto colon = line.find(':'); if (colon == std::string::npos) continue;
            if (lower(line.substr(0, colon)) == "host") {
                ++hosts; host = line.substr(colon + 1); host.erase(0, host.find_first_not_of(" \t"));
                host.erase(host.find_last_not_of(" \t\r") + 1);
            }
        }
        if (hosts != 1 || (host != "127.0.0.1:" + std::to_string(httpPort) && host != "localhost:" + std::to_string(httpPort))) { httpReply(c, 403, "text/plain", "Invalid Host"); return; }
        if (method != "GET") { httpReply(c, 405, "text/plain", "Read-only API"); return; }
        if (path == "/api/status") { httpReply(c, 200, "application/json; charset=utf-8", snapshot().dump()); return; }
        auto a = assets.find(path); if (a == assets.end()) { httpReply(c, 404, "text/plain", "Not found"); return; }
        httpReply(c, 200, path == "/app.js" ? "text/javascript; charset=utf-8" : path == "/style.css" ? "text/css; charset=utf-8" : "text/html; charset=utf-8", a->second);
    }
public:
    ~Manager() { if (tcp != INVALID_SOCKET) closesocket(tcp); if (http != INVALID_SOCKET) closesocket(http); }
    int dashboardPort() const { return httpPort; }
    fs::path webViewProfile() const { return root / "Saved/GSMNative/WebView2" / std::to_string(httpPort); }
    void configure(int argc, wchar_t** argv) {
        root = fs::current_path(); wchar_t exePath[32768]; GetModuleFileNameW(nullptr, exePath, 32768); web = fs::path(exePath).parent_path() / "web";
        server = fs::u8path(env(L"AURA_SERVER_EXE")); editorExe.clear();
        auth = env(L"AURA_GSM_AUTH_TOKEN"); serverAuth = env(L"AURA_GSM_SERVER_AUTH_TOKEN"); if (serverAuth.empty()) serverAuth = auth;
        if (!env(L"AURA_PUBLIC_HOST").empty()) publicHost = env(L"AURA_PUBLIC_HOST");
        if (!env(L"AURA_PERSISTENCE_PROVIDER").empty()) provider = env(L"AURA_PERSISTENCE_PROVIDER");
        const auto bindHost = env(L"AURA_GSM_HOST"); if (!bindHost.empty() && bindHost != "127.0.0.1" && bindHost != "localhost") throw std::runtime_error("Native GSM currently requires loopback AURA_GSM_HOST");
        bool explicitPort = !env(L"AURA_GSM_PORT").empty(); if (explicitPort) tcpPort = portNumber(env(L"AURA_GSM_PORT"));
        fs::path config;
        for (int i = 1; i < argc; ++i) {
            const auto key = utf8(argv[i]);
            if (key == "--headless") continue;
            if (key == "--help") { std::cout << "AuraGSM --project-root PATH [--server-exe PATH] [--editor-exe PATH] [--port 9000] [--web-port 9080] [--public-host HOST] [--persistence-provider NULL] [--config PATH] [--web-root PATH] [--headless]\nDefault: native WebView2 window. --headless: backend only.\n"; exit(0); }
            if (++i >= argc) throw std::runtime_error("Missing option value"); const auto value = utf8(argv[i]);
            if (key == "--project-root") root = argv[i]; else if (key == "--server-exe") server = argv[i]; else if (key == "--editor-exe") editorExe = argv[i];
            else if (key == "--port") { tcpPort = portNumber(value); explicitPort = true; } else if (key == "--web-port") httpPort = portNumber(value);
            else if (key == "--config") config = argv[i]; else if (key == "--web-root") web = argv[i];
            else if (key == "--public-host") publicHost = value; else if (key == "--persistence-provider") provider = value;
            else if (key == "--host" && (value == "127.0.0.1" || value == "localhost")) {} else throw std::runtime_error("Unknown/unsupported option: " + key);
        }
        root = fs::weakly_canonical(fs::absolute(root)); if (config.empty()) config = root / "Content/Config/LevelConfig.json";
        if (!server.empty()) server = fs::absolute(server); if (!editorExe.empty()) editorExe = fs::absolute(editorExe);
        if (publicHost.empty()) throw std::runtime_error("Public host cannot be empty");
        auto cfg = Json::parse(readFile(config)); if (!explicitPort) tcpPort = portNumber(std::to_string(cfg.value("gameServerPort", 9000)));
        std::set<int> ports = {tcpPort, httpPort}; if (tcpPort == httpPort) throw std::runtime_error("TCP and web ports must differ");
        for (auto item : cfg.at("levels")) {
            const auto id = item.at("id").get<std::string>();
            if (id.empty() || id.size() > 100 || id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos || levels.count(id)) throw std::runtime_error("Invalid or duplicate level id");
            const auto map = item.at("mapPath").get<std::string>(); if (map.rfind("/Game/", 0) != 0 || map.find_first_of("?\r\n\"") != std::string::npos) throw std::runtime_error("Invalid map path");
            for (const auto* k : {"port", "queryPort"}) {
                if (item.contains(k) && !item.at(k).is_number_integer()) throw std::runtime_error("Ports must be integers");
                int p = item.value(k, 0); if (std::string(k) == "queryPort" && p == 0) continue; portNumber(std::to_string(p)); if (!ports.insert(p).second) throw std::runtime_error("Duplicate configured port");
            }
            if (!item.contains("launchArgs")) item["launchArgs"] = {"-game", "-server", "-log", "-unattended", "-NoLiveCoding"};
            auto args = item.at("launchArgs").get<std::vector<std::string>>();
            for (const auto& a : args) if (a.find('\0') != std::string::npos) throw std::runtime_error("NUL in launch argument");
            item.value("displayName", id); auto l = std::make_unique<Level>(); l->config = std::move(item); levels.emplace(id, std::move(l));
        }
        if (levels.empty()) throw std::runtime_error("No configured levels");
        assets["/"] = readFile(web / "index.html"); assets["/index.html"] = assets["/"]; assets["/app.js"] = readFile(web / "app.js"); assets["/style.css"] = readFile(web / "style.css");
        tcp = listenOn(tcpPort); http = listenOn(httpPort);
    }
    void run(NativeWindow* window) {
        std::cout << "AuraGSM native | TCP 127.0.0.1:" << tcpPort << " | Dashboard http://127.0.0.1:" << httpPort << " | Ctrl+C stops owned servers" << std::endl;
        while (!InterlockedCompareExchange(&stopping, 0, 0)) {
            if (window && !window->pump()) break;
            refresh();
            for (SOCKET listener : {tcp, http}) {
                SOCKET s = accept(listener, nullptr, nullptr); if (s == INVALID_SOCKET) continue;
                if (clients.size() >= 64) { closesocket(s); continue; }
                u_long nonblocking = 1; ioctlsocket(s, FIONBIO, &nonblocking);
                auto c = std::make_unique<Connection>(); c->socket = s; c->http = listener == http; clients.push_back(std::move(c));
            }
            for (auto it = clients.begin(); it != clients.end();) {
                auto& c = **it; bool done = false;
                if (c.output.empty() && c.waiting.empty()) {
                    char buf[2048]; int n = recv(c.socket, buf, sizeof(buf), 0);
                    if (n > 0) {
                        c.input.append(buf, n); const auto limit = c.http ? 8192u : 4096u;
                        if (c.input.size() > limit) { if (c.http) httpReply(c, 413, "text/plain", "Request too large"); else c.reply(error("Request too large")); }
                        else if (c.http && c.input.find("\r\n\r\n") != std::string::npos) handleHttp(c);
                        else if (!c.http && c.input.find('\n') != std::string::npos) handleProtocol(c);
                    } else if (n == 0 || WSAGetLastError() != WSAEWOULDBLOCK) done = true;
                    if (c.output.empty() && c.waiting.empty() && now() - c.accepted > 5) done = true;
                }
                if (!c.output.empty()) {
                    int n = send(c.socket, c.output.data() + c.sent, static_cast<int>(c.output.size() - c.sent), 0);
                    if (n > 0) c.sent += n; else if (n == 0 || WSAGetLastError() != WSAEWOULDBLOCK) done = true;
                    if (c.sent == c.output.size() || now() - c.writeStarted > 5) done = true;
                }
                if (done) it = clients.erase(it); else ++it;
            }
            Sleep(10);
        }
    }
};
int runApplication(int argc, wchar_t** argv) {
    bool headless = false;
    for (int i = 1; i < argc; ++i) if (wcscmp(argv[i], L"--headless") == 0 || wcscmp(argv[i], L"--help") == 0) headless = true;
    // GUI subsystem avoids an extra console for the normal desktop application.
    // Preserve redirected test/service streams, and attach console streams only if absent.
    if (headless) {
        const bool hasOutput = GetStdHandle(STD_OUTPUT_HANDLE) && GetStdHandle(STD_OUTPUT_HANDLE) != INVALID_HANDLE_VALUE;
        const bool hasError = GetStdHandle(STD_ERROR_HANDLE) && GetStdHandle(STD_ERROR_HANDLE) != INVALID_HANDLE_VALUE;
        if (AttachConsole(ATTACH_PARENT_PROCESS)) {
            FILE* file = nullptr;
            if (!hasOutput) freopen_s(&file, "CONOUT$", "w", stdout);
            if (!hasError) freopen_s(&file, "CONOUT$", "w", stderr);
        }
    }
    WSADATA data{}; if (WSAStartup(MAKEWORD(2, 2), &data)) return 1;
    SetConsoleCtrlHandler(onSignal, TRUE); int result = 0;
    try {
        Manager manager; manager.configure(argc, argv);
        std::unique_ptr<NativeWindow> window;
        if (!headless) { window = std::make_unique<NativeWindow>(); window->open(manager.dashboardPort(), manager.webViewProfile()); }
        manager.run(window.get());
    }
    catch (const std::exception& e) {
        std::cerr << "AuraGSM: " << e.what() << std::endl;
        if (!headless) MessageBoxW(nullptr, wide(e.what()).c_str(), L"Aura GSM — startup or dashboard error", MB_OK | MB_ICONERROR);
        result = 1;
    }
    WSACleanup(); return result;
}
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0; auto argv = CommandLineToArgvW(GetCommandLineW(), &argc); if (!argv) return 1;
    const int result = runApplication(argc, argv); LocalFree(argv); return result;
}
