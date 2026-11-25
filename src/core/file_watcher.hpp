#pragma once

#include <Windows.h>
#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>

namespace moon {

// Zero-polling OS-native directory watcher using ReadDirectoryChangesW + IOCP/Overlapped I/O.
class FileWatcher {
public:
    using ChangeCallback = std::function<void(const std::wstring& fileName)>;

    bool Start(const std::filesystem::path& directory, ChangeCallback onChange);
    void Stop() noexcept;

    ~FileWatcher() { Stop(); }

private:
    void WatchLoop() noexcept;

    std::filesystem::path watchDir_;
    ChangeCallback callback_;
    HANDLE dirHandle_{INVALID_HANDLE_VALUE};
    HANDLE stopEvent_{nullptr};
    std::atomic<bool> running_{false};
    std::thread watchThread_;
};

} // namespace moon
