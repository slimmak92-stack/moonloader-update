#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

namespace moon {

struct RuntimeConfig {
    bool hotReload{true};
    bool loadNativeAddons{true};
    bool isolatedScriptSandboxes{true};
    bool fastBytecodeCache{true};
    bool debugConsole{false};
    int startupDelayMs{0};
    int tickIntervalMs{16};
};

class ConfigManager {
public:
    void LoadOrCreate(const std::filesystem::path& iniPath);
    const RuntimeConfig& Settings() const noexcept { return settings_; }
    std::string GetString(const std::string& section, const std::string& key, const std::string& fallback) const;

private:
    RuntimeConfig settings_{};
    std::unordered_map<std::string, std::string> rawValues_;
};

} // namespace moon
