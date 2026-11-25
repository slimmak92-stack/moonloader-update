#include "runtime/script_manager.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <future>

namespace moon {

std::filesystem::path ScriptManager::ResolveModuleDirectory(HMODULE moduleHandle) {
    wchar_t buffer[MAX_PATH]{};
    const DWORD len = GetModuleFileNameW(moduleHandle, buffer, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(buffer).parent_path();
}

std::string ScriptManager::WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string out(static_cast<std::size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), out.data(), len, nullptr, nullptr);
    return out;
}

std::uint64_t ScriptManager::ComputeFnv1a64(const std::uint8_t* data, std::size_t size) noexcept {
    std::uint64_t hash = 14695981039346656037ULL;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= static_cast<std::uint64_t>(data[i]);
        hash *= 1099511628211ULL;
    }
    return hash;
}

void ScriptManager::LogMessage(int level, const std::string& tag, const std::string& line) noexcept {
    if (logFile_.empty()) {
        return;
    }
    const char* levelLabel = (level == 0) ? "DEBUG" : (level == 1) ? "INFO" : (level == 2) ? "WARN" : "ERROR";
    std::lock_guard<std::mutex> lock(logMutex_);
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, logFile_.c_str(), L"a") == 0 && fp != nullptr) {
        std::fprintf(fp, "[%s][%s] %s\n", levelLabel, tag.c_str(), line.c_str());
        std::fclose(fp);
    }
}

void ScriptManager::Initialize(HMODULE moduleHandle, LuaBridge* bridge, EventBus* bus, bool isolatedSandboxes, bool fastBytecodeCache) {
    bridge_ = bridge;
    eventBus_ = bus;
    isolatedSandboxes_ = isolatedSandboxes;
    fastBytecodeCache_ = fastBytecodeCache;

    gameDir_ = ResolveModuleDirectory(moduleHandle);
    moonloaderDir_ = gameDir_ / L"moonloader";
    cacheDir_ = moonloaderDir_ / L".cache";
    logFile_ = moonloaderDir_ / L"moonloader.log";

    std::error_code ec;
    std::filesystem::create_directories(moonloaderDir_, ec);
    std::filesystem::create_directories(moonloaderDir_ / L"config", ec);
    std::filesystem::create_directories(moonloaderDir_ / L"resource", ec);
    std::filesystem::create_directories(moonloaderDir_ / L"lib", ec);
    std::filesystem::create_directories(moonloaderDir_ / L"addons", ec);
    if (fastBytecodeCache_) {
        std::filesystem::create_directories(cacheDir_, ec);
    }

    FILE* fp = nullptr;
    if (_wfopen_s(&fp, logFile_.c_str(), L"w") == 0 && fp != nullptr) {
        std::fclose(fp);
    }
}

std::filesystem::path ScriptManager::GetCachePathFor(const std::wstring& fileName, std::uint64_t sourceHash) const {
    wchar_t hexBuf[32]{};
    swprintf_s(hexBuf, L"_%016llx.luacache", static_cast<unsigned long long>(sourceHash));
    return cacheDir_ / (fileName + hexBuf);
}

bool ScriptManager::LoadOrExecuteScript(ManagedScript& script) {
    const std::string utf8Name = WideToUtf8(script.name);
    const std::string utf8Path = WideToUtf8(script.path.wstring());

    if (!bridge_ || !isolatedSandboxes_) {
        script.status = ScriptStatus::Loaded;
        return true;
    }

    std::string err;
    if (!bridge_->CreateSandboxState(script.isolatedLuaState, err)) {
        script.status = ScriptStatus::Failed;
        script.lastError = err;
        return false;
    }

    if (!bridge_->IsLuaJitLinked()) {
        script.status = ScriptStatus::Loaded;
        return true;
    }

    // Fast bytecode cache path: read source once, check FNV-1a cached bytecode, avoid reparsing unchanged scripts.
    if (fastBytecodeCache_) {
        std::ifstream in(script.path, std::ios::binary);
        if (in) {
            std::vector<std::uint8_t> sourceBytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            const std::uint64_t hash = ComputeFnv1a64(sourceBytes.data(), sourceBytes.size());
            const auto cachePath = GetCachePathFor(script.name, hash);

            std::error_code ec;
            if (std::filesystem::exists(cachePath, ec)) {
                std::ifstream cacheIn(cachePath, std::ios::binary);
                std::vector<std::uint8_t> cachedBc((std::istreambuf_iterator<char>(cacheIn)), std::istreambuf_iterator<char>());
                if (!cachedBc.empty() && bridge_->ExecuteBytecode(script.isolatedLuaState, cachedBc, utf8Name.c_str(), err)) {
                    script.status = ScriptStatus::Loaded;
                    return true;
                }
            }

            std::vector<std::uint8_t> compiledBc;
            if (bridge_->CompileToBytecode(utf8Path, compiledBc, err)) {
                std::ofstream cacheOut(cachePath, std::ios::binary | std::ios::trunc);
                if (cacheOut) {
                    cacheOut.write(reinterpret_cast<const char*>(compiledBc.data()), static_cast<std::streamsize>(compiledBc.size()));
                }
                if (bridge_->ExecuteBytecode(script.isolatedLuaState, compiledBc, utf8Name.c_str(), err)) {
                    script.status = ScriptStatus::Loaded;
                    return true;
                }
            }
        }
    }

    if (!bridge_->ExecuteScriptFile(script.isolatedLuaState, utf8Path, err)) {
        script.status = ScriptStatus::Failed;
        script.lastError = err;
        return false;
    }

    script.status = ScriptStatus::Loaded;
    return true;
}

void ScriptManager::InitialFastLoad() {
    std::error_code ec;
    if (!std::filesystem::exists(moonloaderDir_, ec)) {
        return;
    }

    std::vector<ManagedScript> discovered;
    discovered.reserve(32);

    for (const auto& entry : std::filesystem::directory_iterator(moonloaderDir_, ec)) {
        if (!entry.is_regular_file(ec)) {
            continue;
        }
        std::wstring ext = entry.path().extension().wstring();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
        if (ext != L".lua" && ext != L".luac") {
            continue;
        }

        ManagedScript item;
        item.name = entry.path().filename().wstring();
        item.path = entry.path();
        item.lastWriteTime = entry.last_write_time(ec);
        item.fileSize = entry.file_size(ec);
        discovered.push_back(std::move(item));
    }

    // Parallel bytecode pre-warming when multiple scripts exist and LuaJIT is enabled.
    if (fastBytecodeCache_ && bridge_ && bridge_->IsLuaJitLinked() && discovered.size() > 1) {
        std::vector<std::future<void>> jobs;
        jobs.reserve(discovered.size());
        for (const auto& s : discovered) {
            jobs.push_back(std::async(std::launch::async, [this, path = s.path, name = s.name]() {
                std::ifstream in(path, std::ios::binary);
                if (!in) return;
                std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                const std::uint64_t hash = ComputeFnv1a64(bytes.data(), bytes.size());
                const auto cachePath = GetCachePathFor(name, hash);
                std::error_code localEc;
                if (!std::filesystem::exists(cachePath, localEc)) {
                    std::vector<std::uint8_t> bc;
                    std::string err;
                    if (bridge_->CompileToBytecode(WideToUtf8(path.wstring()), bc, err)) {
                        std::ofstream out(cachePath, std::ios::binary | std::ios::trunc);
                        if (out) {
                            out.write(reinterpret_cast<const char*>(bc.data()), static_cast<std::streamsize>(bc.size()));
                        }
                    }
                }
            }));
        }
        for (auto& j : jobs) {
            j.wait();
        }
    }

    for (auto& script : discovered) {
        const std::string utf8Name = WideToUtf8(script.name);
        if (LoadOrExecuteScript(script)) {
            LogMessage(1, "FastLoader", "Loaded: " + utf8Name);
            if (eventBus_) {
                eventBus_->Emit("script.loaded", utf8Name.c_str());
            }
        } else {
            LogMessage(3, "FastLoader", "Failed: " + utf8Name + " -> " + script.lastError);
        }
    }

    std::lock_guard<std::mutex> lock(scriptsMutex_);
    scripts_ = std::move(discovered);
}

void ScriptManager::ReloadSingleScript(const std::wstring& fileName) {
    const auto fullPath = moonloaderDir_ / fileName;
    std::error_code ec;
    if (!std::filesystem::exists(fullPath, ec)) {
        return;
    }

    const auto writeTime = std::filesystem::last_write_time(fullPath, ec);
    const auto fileSize = std::filesystem::file_size(fullPath, ec);
    const std::string utf8Name = WideToUtf8(fileName);

    std::lock_guard<std::mutex> lock(scriptsMutex_);
    for (auto& script : scripts_) {
        if (script.name == fileName) {
            if (script.lastWriteTime == writeTime && script.fileSize == fileSize) {
                return;
            }
            script.lastWriteTime = writeTime;
            script.fileSize = fileSize;
            if (bridge_ && script.isolatedLuaState) {
                bridge_->DestroySandboxState(script.isolatedLuaState);
            }
            if (LoadOrExecuteScript(script)) {
                LogMessage(1, "InstantReload", "Hot-reloaded: " + utf8Name);
                if (eventBus_) {
                    eventBus_->Emit("script.reloaded", utf8Name.c_str());
                }
            } else {
                LogMessage(3, "InstantReload", "Reload error in " + utf8Name + ": " + script.lastError);
            }
            return;
        }
    }

    // Newly dropped script detected by ReadDirectoryChangesW.
    ManagedScript added;
    added.name = fileName;
    added.path = fullPath;
    added.lastWriteTime = writeTime;
    added.fileSize = fileSize;
    if (LoadOrExecuteScript(added)) {
        LogMessage(1, "InstantReload", "Loaded new script: " + utf8Name);
        if (eventBus_) {
            eventBus_->Emit("script.loaded", utf8Name.c_str());
        }
    }
    scripts_.push_back(std::move(added));
}

void ScriptManager::TickScripts(double deltaSeconds) noexcept {
    if (eventBus_) {
        eventBus_->Emit("runtime.tick", &deltaSeconds);
    }
}

void ScriptManager::UnloadAll() noexcept {
    std::lock_guard<std::mutex> lock(scriptsMutex_);
    for (auto& script : scripts_) {
        if (bridge_ && script.isolatedLuaState) {
            bridge_->DestroySandboxState(script.isolatedLuaState);
        }
        script.status = ScriptStatus::Unloaded;
    }
    scripts_.clear();
}

std::uint32_t ScriptManager::LoadedCount() const noexcept {
    std::lock_guard<std::mutex> lock(scriptsMutex_);
    std::uint32_t count = 0;
    for (const auto& s : scripts_) {
        if (s.status == ScriptStatus::Loaded) ++count;
    }
    return count;
}

std::uint32_t ScriptManager::FailedCount() const noexcept {
    std::lock_guard<std::mutex> lock(scriptsMutex_);
    std::uint32_t count = 0;
    for (const auto& s : scripts_) {
        if (s.status == ScriptStatus::Failed) ++count;
    }
    return count;
}

} // namespace moon
