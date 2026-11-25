#pragma once

#include "openmoon/plugin_api.h"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace moon {

class LuaBridge {
public:
    void RegisterModuleInitializer(std::string moduleName, OpenMoonLuaRegisterFn fn);
    bool CreateSandboxState(void*& outState, std::string& outError) const;
    bool CompileToBytecode(const std::string& utf8SourcePath, std::vector<std::uint8_t>& outBytecode, std::string& outError) const;
    bool ExecuteBytecode(void* state, const std::vector<std::uint8_t>& bytecode, const char* chunkName, std::string& outError) const;
    bool ExecuteScriptFile(void* state, const std::string& utf8Path, std::string& outError) const;
    void DestroySandboxState(void*& state) const noexcept;
    bool IsLuaJitLinked() const noexcept;

private:
    std::vector<std::pair<std::string, OpenMoonLuaRegisterFn>> moduleInitializers_;
};

} // namespace moon
