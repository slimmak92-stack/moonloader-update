#include "openmoon/plugin_api.h"
#include "core/addon_loader.hpp"
#include "core/config.hpp"
#include "core/event_bus.hpp"
#include "core/file_watcher.hpp"
#include "modules/builtin_modules.hpp"
#include "runtime/lua_bridge.hpp"
#include "runtime/script_manager.hpp"

#include <Windows.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

namespace {

struct EngineState {
    std::atomic<bool> running{true};
    std::atomic<std::uint64_t> startupMicroseconds{0};
    HANDLE stopEvent{nullptr};
    HMODULE selfModule{nullptr};
    std::thread workerThread;

    moon::ConfigManager config;
    moon::EventBus eventBus;
    moon::LuaBridge luaBridge;
    moon::ScriptManager scriptManager;
    moon::AddonLoader addonLoader;
    moon::FileWatcher fileWatcher;
};

EngineState g_engine;
thread_local std::string tls_configBuffer;

void OPENMOON_CALL Host_Log(int level, const char* tag, const char* message) {
    g_engine.scriptManager.LogMessage(level, tag ? tag : "Addon", message ? message : "");
}

int OPENMOON_CALL Host_SubscribeEvent(const char* eventName, OpenMoonEventCallback cb, void* userData) {
    return g_engine.eventBus.Subscribe(eventName ? eventName : "", cb, userData);
}

void OPENMOON_CALL Host_UnsubscribeEvent(int subscriptionId) {
    g_engine.eventBus.Unsubscribe(subscriptionId);
}

void OPENMOON_CALL Host_EmitEvent(const char* eventName, const void* payload) {
    g_engine.eventBus.Emit(eventName ? eventName : "", payload);
}

void OPENMOON_CALL Host_RegisterLuaModule(const char* moduleName, OpenMoonLuaRegisterFn registerFn) {
    g_engine.luaBridge.RegisterModuleInitializer(moduleName ? moduleName : "", registerFn);
}

const char* OPENMOON_CALL Host_GetConfigString(const char* section, const char* key, const char* fallback) {
    tls_configBuffer = g_engine.config.GetString(
        section ? section : "",
        key ? key : "",
        fallback ? fallback : "");
    return tls_configBuffer.c_str();
}

void OPENMOON_CALL Host_GetTelemetry(OpenMoonTelemetry* outTelemetry) {
    if (!outTelemetry) return;
    outTelemetry->startupMicroseconds = g_engine.startupMicroseconds.load(std::memory_order_acquire);
    outTelemetry->loadedScripts = g_engine.scriptManager.LoadedCount();
    outTelemetry->failedScripts = g_engine.scriptManager.FailedCount();
    outTelemetry->loadedAddons = static_cast<std::uint32_t>(g_engine.addonLoader.Addons().size());
}

const OpenMoonHostApi g_hostApi{
    OPENMOON_ABI_VERSION,
    sizeof(OpenMoonHostApi),
    &Host_Log,
    &Host_SubscribeEvent,
    &Host_UnsubscribeEvent,
    &Host_EmitEvent,
    &Host_RegisterLuaModule,
    &Host_GetConfigString,
    &Host_GetTelemetry,
};

void RuntimeThreadMain() noexcept {
    const auto bootStart = std::chrono::steady_clock::now();

    moon::RegisterBuiltinModules(&g_hostApi);
    g_engine.scriptManager.Initialize(
        g_engine.selfModule,
        &g_engine.luaBridge,
        &g_engine.eventBus,
        true,
        true);

    const auto iniPath = g_engine.scriptManager.MoonloaderDirectory() / L"config" / L"OpenMoonLoader.ini";
    g_engine.config.LoadOrCreate(iniPath);

    // Zero artificial startup delay by default (StartupDelayMs=0).
    const int delayMs = g_engine.config.Settings().startupDelayMs;
    if (delayMs > 0 && WaitForSingleObject(g_engine.stopEvent, static_cast<DWORD>(delayMs)) == WAIT_OBJECT_0) {
        return;
    }

    if (g_engine.config.Settings().loadNativeAddons) {
        g_engine.addonLoader.LoadFromDirectory(
            g_engine.scriptManager.MoonloaderDirectory() / L"addons",
            &g_hostApi);
    }

    g_engine.scriptManager.InitialFastLoad();

    if (g_engine.config.Settings().hotReload) {
        g_engine.fileWatcher.Start(g_engine.scriptManager.MoonloaderDirectory(), [](const std::wstring& changedFile) {
            g_engine.scriptManager.ReloadSingleScript(changedFile);
        });
    }

    const auto bootEnd = std::chrono::steady_clock::now();
    const auto elapsedUs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(bootEnd - bootStart).count());
    g_engine.startupMicroseconds.store(elapsedUs, std::memory_order_release);

    char bootMsg[160]{};
    std::snprintf(
        bootMsg,
        sizeof(bootMsg),
        "Turbo boot completed in %.2f ms (%u scripts, %zu addons).",
        static_cast<double>(elapsedUs) / 1000.0,
        g_engine.scriptManager.LoadedCount(),
        g_engine.addonLoader.Addons().size());
    g_engine.scriptManager.LogMessage(1, "OpenMoonLoader", bootMsg);
    g_engine.eventBus.Emit("runtime.ready", nullptr);

    auto lastTick = std::chrono::steady_clock::now();

    while (g_engine.running.load(std::memory_order_acquire)) {
        const auto now = std::chrono::steady_clock::now();
        const double dt = std::chrono::duration<double>(now - lastTick).count();
        lastTick = now;

        g_engine.scriptManager.TickScripts(dt);

        const DWORD sleepMs = static_cast<DWORD>(g_engine.config.Settings().tickIntervalMs);
        if (WaitForSingleObject(g_engine.stopEvent, sleepMs) == WAIT_OBJECT_0) {
            break;
        }
    }

    g_engine.fileWatcher.Stop();
    g_engine.eventBus.Emit("runtime.shutdown", nullptr);
    g_engine.scriptManager.UnloadAll();
    g_engine.addonLoader.UnloadAll();
    g_engine.eventBus.Clear();
}

void StartEngine(HMODULE hModule) noexcept {
    g_engine.selfModule = hModule;
    g_engine.running.store(true, std::memory_order_release);
    g_engine.stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (g_engine.stopEvent != nullptr) {
        g_engine.workerThread = std::thread(RuntimeThreadMain);
    }
}

void StopEngine() noexcept {
    g_engine.running.store(false, std::memory_order_release);
    if (g_engine.stopEvent != nullptr) {
        SetEvent(g_engine.stopEvent);
    }
    if (g_engine.workerThread.joinable()) {
        g_engine.workerThread.join();
    }
    if (g_engine.stopEvent != nullptr) {
        CloseHandle(g_engine.stopEvent);
        g_engine.stopEvent = nullptr;
    }
}

} // namespace

extern "C" OPENMOON_API const OpenMoonHostApi* OPENMOON_CALL OpenMoon_GetHostApi(void) {
    return &g_hostApi;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        StartEngine(hModule);
        break;
    case DLL_PROCESS_DETACH:
        StopEngine();
        break;
    }
    return TRUE;
}
