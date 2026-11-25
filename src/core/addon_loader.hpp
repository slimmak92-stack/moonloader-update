#pragma once

#include "openmoon/plugin_api.h"
#include <Windows.h>
#include <filesystem>
#include <string>
#include <vector>

namespace moon {

struct LoadedAddon {
    std::wstring fileName;
    HMODULE handle{nullptr};
    std::string name;
    std::string version;
    std::string author;
    OpenMoonAddonShutdownFn shutdownFn{nullptr};
};

class AddonLoader {
public:
    void LoadFromDirectory(const std::filesystem::path& addonsDir, const OpenMoonHostApi* hostApi);
    void UnloadAll() noexcept;
    const std::vector<LoadedAddon>& Addons() const noexcept { return addons_; }

private:
    std::vector<LoadedAddon> addons_;
};

} // namespace moon
