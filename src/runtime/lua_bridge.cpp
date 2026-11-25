#include "runtime/lua_bridge.hpp"

#if defined(OPENMOON_HAS_LUAJIT)
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
#endif

namespace moon {

void LuaBridge::RegisterModuleInitializer(std::string moduleName, OpenMoonLuaRegisterFn fn) {
    if (!moduleName.empty() && fn != nullptr) {
        moduleInitializers_.emplace_back(std::move(moduleName), fn);
    }
}

bool LuaBridge::IsLuaJitLinked() const noexcept {
#if defined(OPENMOON_HAS_LUAJIT)
    return true;
#else
    return false;
#endif
}

bool LuaBridge::CreateSandboxState(void*& outState, std::string& outError) const {
#if defined(OPENMOON_HAS_LUAJIT)
    lua_State* L = luaL_newstate();
    if (!L) {
        outError = "Failed to allocate isolated LuaJIT state.";
        return false;
    }
    luaL_openlibs(L);

    for (const auto& [name, regFn] : moduleInitializers_) {
        if (regFn) {
            regFn(L);
        }
        (void)name;
    }

    outState = L;
    return true;
#else
    outState = nullptr;
    outError = "Standalone C++ runtime active (compile with -DOPENMOON_WITH_LUAJIT=ON for Lua execution).";
    return true;
#endif
}

#if defined(OPENMOON_HAS_LUAJIT)
namespace {
int BytecodeWriter(lua_State*, const void* p, size_t sz, void* ud) {
    auto* vec = static_cast<std::vector<std::uint8_t>*>(ud);
    const auto* bytes = static_cast<const std::uint8_t*>(p);
    vec->insert(vec->end(), bytes, bytes + sz);
    return 0;
}
} // namespace
#endif

bool LuaBridge::CompileToBytecode(const std::string& utf8SourcePath, std::vector<std::uint8_t>& outBytecode, std::string& outError) const {
#if defined(OPENMOON_HAS_LUAJIT)
    lua_State* compilerState = luaL_newstate();
    if (!compilerState) {
        outError = "Failed to allocate compiler state.";
        return false;
    }

    if (luaL_loadfile(compilerState, utf8SourcePath.c_str()) != 0) {
        const char* err = lua_tostring(compilerState, -1);
        outError = err ? err : "Syntax error during compilation.";
        lua_close(compilerState);
        return false;
    }

    outBytecode.clear();
    const int dumpRes = lua_dump(compilerState, &BytecodeWriter, &outBytecode);
    lua_close(compilerState);

    if (dumpRes != 0) {
        outError = "lua_dump failed.";
        return false;
    }
    return true;
#else
    (void)utf8SourcePath;
    (void)outBytecode;
    outError = "LuaJIT not linked.";
    return false;
#endif
}

bool LuaBridge::ExecuteBytecode(void* state, const std::vector<std::uint8_t>& bytecode, const char* chunkName, std::string& outError) const {
#if defined(OPENMOON_HAS_LUAJIT)
    auto* L = static_cast<lua_State*>(state);
    if (!L || bytecode.empty()) {
        outError = "Invalid LuaJIT state or empty bytecode.";
        return false;
    }
    if (luaL_loadbuffer(L, reinterpret_cast<const char*>(bytecode.data()), bytecode.size(), chunkName) != 0) {
        const char* err = lua_tostring(L, -1);
        outError = err ? err : "Failed to load cached bytecode.";
        lua_pop(L, 1);
        return false;
    }
    if (lua_pcall(L, 0, LUA_MULTRET, 0) != 0) {
        const char* err = lua_tostring(L, -1);
        outError = err ? err : "Runtime error executing cached bytecode.";
        lua_pop(L, 1);
        return false;
    }
    return true;
#else
    (void)state;
    (void)bytecode;
    (void)chunkName;
    outError = "LuaJIT not linked.";
    return false;
#endif
}

bool LuaBridge::ExecuteScriptFile(void* state, const std::string& utf8Path, std::string& outError) const {
#if defined(OPENMOON_HAS_LUAJIT)
    auto* L = static_cast<lua_State*>(state);
    if (!L) {
        outError = "Null LuaJIT state.";
        return false;
    }
    if (luaL_dofile(L, utf8Path.c_str()) != 0) {
        const char* err = lua_tostring(L, -1);
        outError = err ? err : "Unknown Lua error.";
        lua_pop(L, 1);
        return false;
    }
    return true;
#else
    (void)state;
    (void)utf8Path;
    outError = "LuaJIT not linked.";
    return false;
#endif
}

void LuaBridge::DestroySandboxState(void*& state) const noexcept {
#if defined(OPENMOON_HAS_LUAJIT)
    auto* L = static_cast<lua_State*>(state);
    if (L) {
        lua_close(L);
    }
#endif
    state = nullptr;
}

} // namespace moon
