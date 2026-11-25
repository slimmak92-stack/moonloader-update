#include "core/config.hpp"

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace moon {
namespace {

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

bool ParseBool(const std::string& val, bool fallback) {
    const auto lower = ToLower(val);
    if (lower == "1" || lower == "true" || lower == "yes" || lower == "on") return true;
    if (lower == "0" || lower == "false" || lower == "no" || lower == "off") return false;
    return fallback;
}

} // namespace

void ConfigManager::LoadOrCreate(const std::filesystem::path& iniPath) {
    std::error_code ec;
    if (!std::filesystem::exists(iniPath, ec)) {
        FILE* fp = nullptr;
        if (_wfopen_s(&fp, iniPath.c_str(), L"w") == 0 && fp != nullptr) {
            constexpr const char kDefaultIni[] = R"([Runtime]
HotReload=true
LoadNativeAddons=true
IsolatedScriptSandboxes=true
FastBytecodeCache=true
DebugConsole=false
StartupDelayMs=0
TickIntervalMs=16
)";
            std::fwrite(kDefaultIni, 1, sizeof(kDefaultIni) - 1, fp);
            std::fclose(fp);
        }
    }

    const std::wstring wPath = iniPath.wstring();
    auto readStr = [&](const wchar_t* sec, const wchar_t* key, const wchar_t* def) -> std::string {
        wchar_t buf[256]{};
        GetPrivateProfileStringW(sec, key, def, buf, 256, wPath.c_str());
        int len = WideCharToMultiByte(CP_UTF8, 0, buf, -1, nullptr, 0, nullptr, nullptr);
        if (len <= 1) return {};
        std::string out(static_cast<std::size_t>(len - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, buf, -1, out.data(), len, nullptr, nullptr);
        return out;
    };

    settings_.hotReload = ParseBool(readStr(L"Runtime", L"HotReload", L"true"), true);
    settings_.loadNativeAddons = ParseBool(readStr(L"Runtime", L"LoadNativeAddons", L"true"), true);
    settings_.isolatedScriptSandboxes = ParseBool(readStr(L"Runtime", L"IsolatedScriptSandboxes", L"true"), true);
    settings_.fastBytecodeCache = ParseBool(readStr(L"Runtime", L"FastBytecodeCache", L"true"), true);
    settings_.debugConsole = ParseBool(readStr(L"Runtime", L"DebugConsole", L"false"), false);
    settings_.startupDelayMs = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Runtime", L"StartupDelayMs", 0, wPath.c_str())), 0, 5000);
    settings_.tickIntervalMs = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Runtime", L"TickIntervalMs", 16, wPath.c_str())), 1, 250);

    rawValues_["runtime.hotreload"] = settings_.hotReload ? "true" : "false";
    rawValues_["runtime.loadnativeaddons"] = settings_.loadNativeAddons ? "true" : "false";
    rawValues_["runtime.isolatedscriptsandboxes"] = settings_.isolatedScriptSandboxes ? "true" : "false";
    rawValues_["runtime.fastbytecodecache"] = settings_.fastBytecodeCache ? "true" : "false";
}

std::string ConfigManager::GetString(const std::string& section, const std::string& key, const std::string& fallback) const {
    const auto composite = ToLower(section + "." + key);
    const auto it = rawValues_.find(composite);
    return (it != rawValues_.end()) ? it->second : fallback;
}

} // namespace moon
