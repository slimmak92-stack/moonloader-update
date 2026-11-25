#pragma once

#include "openmoon/plugin_api.h"
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace moon {

class EventBus {
public:
    int Subscribe(const std::string& eventName, OpenMoonEventCallback cb, void* userData);
    void Unsubscribe(int subscriptionId);
    void Emit(const std::string& eventName, const void* payload) const;
    void Clear();

private:
    struct Listener {
        int id{0};
        OpenMoonEventCallback callback{nullptr};
        void* userData{nullptr};
    };

    mutable std::shared_mutex mutex_;
    int nextId_{1};
    std::unordered_map<std::string, std::vector<Listener>> listeners_;
};

} // namespace moon
