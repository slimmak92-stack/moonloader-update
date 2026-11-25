#include "core/addon_loader.hpp"

namespace moon {

void AddonLoader::LoadFromDirectory(const std::filesystem::path& addonsDir, const OpenMoonHostApi* hostApi) {
    std::error_code ec;
    if (!hostApi || !std::filesystem::exists(addonsDir, ec)) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(addonsDir, ec)) {
        if (!entry.is_regular_file(ec) || entry.path().extension() != L".dll") {
            continue;
        }

        HMODULE mod = LoadLibraryW(entry.path().c_str());
        if (!mod) {
            continue;
        }

        auto initFn = reinterpret_cast<OpenMoonAddonInitFn>(GetProcAddress(mod, "OpenMoonAddon_Init"));
        auto shutdownFn = reinterpret_cast<OpenMoonAddonShutdownFn>(GetProcAddress(mod, "OpenMoonAddon_Shutdown"));
        if (!initFn) {
            FreeLibrary(mod);
            continue;
        }

        const OpenMoonAddonInfo* info = initFn(hostApi);
        if (!info || info->abiVersion != OPENMOON_ABI_VERSION) {
            if (shutdownFn) {
                shutdownFn();
            }
            FreeLibrary(mod);
            continue;
        }

        LoadedAddon addon;
        addon.fileName = entry.path().filename().wstring();
        addon.handle = mod;
        addon.name = info->name ? info->name : "UnnamedAddon";
        addon.version = info->version ? info->version : "0.0.0";
        addon.author = info->author ? info->author : "Unknown";
        addon.shutdownFn = shutdownFn;
        addons_.push_back(std::move(addon));

        if (hostApi->Log) {
            const std::string msg = "Loaded native addon: " + addons_.back().name + " v" + addons_.back().version;
            hostApi->Log(1, "AddonLoader", msg.c_str());
        }
    }
}

void AddonLoader::UnloadAll() noexcept {
    for (auto it = addons_.rbegin(); it != addons_.rend(); ++it) {
        if (it->shutdownFn) {
            it->shutdownFn();
        }
        if (it->handle) {
            FreeLibrary(it->handle);
        }
    }
    addons_.clear();
}

} // namespace moon
