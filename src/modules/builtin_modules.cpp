#include "modules/builtin_modules.hpp"

#include <Windows.h>

#if defined(OPENMOON_HAS_LUAJIT)
extern "C" {
#include "lua.h"
#include "lauxlib.h"
}
#endif

namespace moon {
namespace {

const OpenMoonHostApi* g_host = nullptr;

#if defined(OPENMOON_HAS_LUAJIT)
int Lua_IsSampAvailable(lua_State* L) {
    static const bool s_cachedSamp = (GetModuleHandleA("samp.dll") != nullptr);
    const bool loaded = s_cachedSamp || (GetModuleHandleA("samp.dll") != nullptr);
    lua_pushboolean(L, loaded ? 1 : 0);
    return 1;
}

int Lua_GetOpenMoonVersion(lua_State* L) {
    lua_pushstring(L, "OpenMoonLoader-Turbo/2.0.0");
    return 1;
}

int Lua_GetStartupMicroseconds(lua_State* L) {
    OpenMoonTelemetry telemetry{};
    if (g_host && g_host->GetTelemetry) {
        g_host->GetTelemetry(&telemetry);
    }
    lua_pushnumber(L, static_cast<lua_Number>(telemetry.startupMicroseconds));
    return 1;
}

int RegisterCoreApi(void* rawState) {
    auto* L = static_cast<lua_State*>(rawState);
    if (!L) return 0;

    lua_register(L, "isSampAvailable", Lua_IsSampAvailable);
    lua_register(L, "getOpenMoonVersion", Lua_GetOpenMoonVersion);
    lua_register(L, "getStartupMicroseconds", Lua_GetStartupMicroseconds);
    return 1;
}
#else
int RegisterCoreApi(void*) {
    return 1;
}
#endif

} // namespace

void RegisterBuiltinModules(const OpenMoonHostApi* hostApi) {
    g_host = hostApi;
    if (g_host && g_host->RegisterLuaModule) {
        g_host->RegisterLuaModule("openmoon.core", RegisterCoreApi);
    }
}

} // namespace moon
