#include "openmoon/plugin_api.h"
#include <cstdio>

namespace {

const OpenMoonHostApi* g_host = nullptr;
int g_readySubId = 0;

void OPENMOON_CALL OnRuntimeReady(const char*, const void*, void*) {
    if (g_host && g_host->GetTelemetry && g_host->Log) {
        OpenMoonTelemetry stats{};
        g_host->GetTelemetry(&stats);
        char buf[128]{};
        std::snprintf(buf, sizeof(buf), "Telemetry: boot=%.2f ms, scripts=%u",
            static_cast<double>(stats.startupMicroseconds) / 1000.0, stats.loadedScripts);
        g_host->Log(1, "SampleNativeAddon", buf);
    }
}

const OpenMoonAddonInfo g_addonInfo{
    OPENMOON_ABI_VERSION,
    "SampleNativeAddon",
    "2.0.0",
    "OpenMoonLoader Contributors",
};

} // namespace

extern "C" __declspec(dllexport) const OpenMoonAddonInfo* OPENMOON_CALL OpenMoonAddon_Init(const OpenMoonHostApi* host) {
    g_host = host;
    if (g_host && g_host->SubscribeEvent) {
        g_readySubId = g_host->SubscribeEvent("runtime.ready", &OnRuntimeReady, nullptr);
    }
    return &g_addonInfo;
}

extern "C" __declspec(dllexport) void OPENMOON_CALL OpenMoonAddon_Shutdown(void) {
    if (g_host && g_host->UnsubscribeEvent && g_readySubId != 0) {
        g_host->UnsubscribeEvent(g_readySubId);
        g_readySubId = 0;
    }
}
