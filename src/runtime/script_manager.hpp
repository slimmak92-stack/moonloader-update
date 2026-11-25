#pragma once

#include "core/event_bus.hpp"
#include "runtime/lua_bridge.hpp"
#include <Windows.h>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace moon {

enum class ScriptStatus {
    Loaded,
    Failed,
    Unloaded
};

struct ManagedScript {
    std::wstring name;
    std::filesystem::path path;
    std::filesystem::file_time_type lastWriteTime{};
    std::uintmax_t fileSize{0};
    ScriptStatus status{ScriptStatus::Unloaded};
    void* isolatedLuaState{nullptr};
    std::string lastError;
};

class ScriptManager {
public:
    void Initialize(HMODULE moduleHandle, LuaBridge* bridge, EventBus* bus, bool isolatedSandboxes, bool fastBytecodeCache);
    void InitialFastLoad();
    void ReloadSingleScript(const std::wstring& fileName);
    void TickScripts(double deltaSeconds) noexcept;
    void UnloadAll() noexcept;
    void LogMessage(int level, const std::string& tag, const std::string& line) noexcept;

    std::uint32_t LoadedCount() const noexcept;
    std::uint32_t FailedCount() const noexcept;
    const std::filesystem::path& GameDirectory() const noexcept { return gameDir_; }
    const std::filesystem::path& MoonloaderDirectory() const noexcept { return moonloaderDir_; }

private:
    static std::filesystem::path ResolveModuleDirectory(HMODULE moduleHandle);
    static std::string WideToUtf8(const std::wstring& w);
    static std::uint64_t ComputeFnv1a64(const std::uint8_t* data, std::size_t size) noexcept;

    bool LoadOrExecuteScript(ManagedScript& script);
    std::filesystem::path GetCachePathFor(const std::wstring& fileName, std::uint64_t sourceHash) const;

    std::filesystem::path gameDir_;
    std::filesystem::path moonloaderDir_;
    std::filesystem::path cacheDir_;
    std::filesystem::path logFile_;
    LuaBridge* bridge_{nullptr};
    EventBus* eventBus_{nullptr};
    bool isolatedSandboxes_{true};
    bool fastBytecodeCache_{true};

    mutable std::mutex scriptsMutex_;
    mutable std::mutex logMutex_;
    std::vector<ManagedScript> scripts_;
};

} // namespace moon
