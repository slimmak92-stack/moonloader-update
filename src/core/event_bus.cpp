#include "core/event_bus.hpp"

#include <algorithm>
#include <mutex>

namespace moon {

int EventBus::Subscribe(const std::string& eventName, OpenMoonEventCallback cb, void* userData) {
    if (!cb || eventName.empty()) {
        return 0;
    }
    std::unique_lock<std::shared_mutex> lock(mutex_);
    const int id = nextId_++;
    listeners_[eventName].push_back(Listener{id, cb, userData});
    return id;
}

void EventBus::Unsubscribe(int subscriptionId) {
    if (subscriptionId <= 0) {
        return;
    }
    std::unique_lock<std::shared_mutex> lock(mutex_);
    for (auto& [name, vec] : listeners_) {
        vec.erase(
            std::remove_if(vec.begin(), vec.end(), [subscriptionId](const Listener& l) {
                return l.id == subscriptionId;
            }),
            vec.end());
    }
}

void EventBus::Emit(const std::string& eventName, const void* payload) const {
    // Fast shared-lock lookup: zero heap allocations when no listeners are registered.
    std::shared_lock<std::shared_mutex> lock(mutex_);
    const auto it = listeners_.find(eventName);
    if (it == listeners_.end() || it->second.empty()) {
        return;
    }

    for (const auto& listener : it->second) {
        if (listener.callback) {
            listener.callback(eventName.c_str(), payload, listener.userData);
        }
    }
}

void EventBus::Clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    listeners_.clear();
}

} // namespace moon
