#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
  #if defined(OPENMOON_EXPORTS)
    #define OPENMOON_API __declspec(dllexport)
  #else
    #define OPENMOON_API __declspec(dllimport)
  #endif
  #define OPENMOON_CALL __cdecl
#else
  #define OPENMOON_API
  #define OPENMOON_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define OPENMOON_ABI_VERSION 2

typedef struct OpenMoonHostApi OpenMoonHostApi;
typedef struct OpenMoonAddonInfo {
    uint32_t abiVersion;
    const char* name;
    const char* version;
    const char* author;
} OpenMoonAddonInfo;

typedef struct OpenMoonTelemetry {
    uint64_t startupMicroseconds;
    uint32_t loadedScripts;
    uint32_t failedScripts;
    uint32_t loadedAddons;
} OpenMoonTelemetry;

typedef void (OPENMOON_CALL *OpenMoonEventCallback)(const char* eventName, const void* payload, void* userData);
typedef int  (OPENMOON_CALL *OpenMoonLuaRegisterFn)(void* luaState);

struct OpenMoonHostApi {
    uint32_t abiVersion;
    uint32_t structSize;

    void (OPENMOON_CALL *Log)(int level, const char* tag, const char* message);
    int  (OPENMOON_CALL *SubscribeEvent)(const char* eventName, OpenMoonEventCallback cb, void* userData);
    void (OPENMOON_CALL *UnsubscribeEvent)(int subscriptionId);
    void (OPENMOON_CALL *EmitEvent)(const char* eventName, const void* payload);
    void (OPENMOON_CALL *RegisterLuaModule)(const char* moduleName, OpenMoonLuaRegisterFn registerFn);
    const char* (OPENMOON_CALL *GetConfigString)(const char* section, const char* key, const char* fallback);
    void (OPENMOON_CALL *GetTelemetry)(OpenMoonTelemetry* outTelemetry);
};

OPENMOON_API const OpenMoonHostApi* OPENMOON_CALL OpenMoon_GetHostApi(void);

typedef const OpenMoonAddonInfo* (OPENMOON_CALL *OpenMoonAddonInitFn)(const OpenMoonHostApi* host);
typedef void (OPENMOON_CALL *OpenMoonAddonShutdownFn)(void);

#ifdef __cplusplus
}
#endif
