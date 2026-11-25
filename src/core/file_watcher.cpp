#include "core/file_watcher.hpp"

#include <algorithm>
#include <vector>

namespace moon {

bool FileWatcher::Start(const std::filesystem::path& directory, ChangeCallback onChange) {
    Stop();

    watchDir_ = directory;
    callback_ = std::move(onChange);

    dirHandle_ = CreateFileW(
        watchDir_.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr);

    if (dirHandle_ == INVALID_HANDLE_VALUE) {
        return false;
    }

    stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!stopEvent_) {
        CloseHandle(dirHandle_);
        dirHandle_ = INVALID_HANDLE_VALUE;
        return false;
    }

    running_.store(true, std::memory_order_release);
    watchThread_ = std::thread(&FileWatcher::WatchLoop, this);
    return true;
}

void FileWatcher::Stop() noexcept {
    running_.store(false, std::memory_order_release);

    if (stopEvent_) {
        SetEvent(stopEvent_);
    }
    if (dirHandle_ != INVALID_HANDLE_VALUE) {
        CancelIoEx(dirHandle_, nullptr);
    }
    if (watchThread_.joinable()) {
        watchThread_.join();
    }
    if (dirHandle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(dirHandle_);
        dirHandle_ = INVALID_HANDLE_VALUE;
    }
    if (stopEvent_) {
        CloseHandle(stopEvent_);
        stopEvent_ = nullptr;
    }
}

void FileWatcher::WatchLoop() noexcept {
    alignas(DWORD) std::uint8_t buffer[16 * 1024]{};
    HANDLE ioEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ioEvent) {
        return;
    }

    OVERLAPPED overlapped{};
    overlapped.hEvent = ioEvent;

    while (running_.load(std::memory_order_acquire)) {
        ResetEvent(ioEvent);
        DWORD bytesReturned = 0;

        const BOOL ok = ReadDirectoryChangesW(
            dirHandle_,
            buffer,
            sizeof(buffer),
            FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
            &bytesReturned,
            &overlapped,
            nullptr);

        if (!ok) {
            break;
        }

        HANDLE waitHandles[2] = {stopEvent_, ioEvent};
        const DWORD waitRes = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
        if (waitRes == WAIT_OBJECT_0) {
            break;
        }
        if (waitRes != WAIT_OBJECT_0 + 1) {
            break;
        }

        if (!GetOverlappedResult(dirHandle_, &overlapped, &bytesReturned, FALSE) || bytesReturned == 0) {
            continue;
        }

        std::size_t offset = 0;
        while (offset < bytesReturned) {
            const auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(buffer + offset);
            std::wstring fileName(info->FileName, info->FileNameLength / sizeof(wchar_t));

            const auto extPos = fileName.find_last_of(L'.');
            if (extPos != std::wstring::npos) {
                std::wstring ext = fileName.substr(extPos);
                std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
                if ((ext == L".lua" || ext == L".luac") && callback_) {
                    callback_(fileName);
                }
            }

            if (info->NextEntryOffset == 0) {
                break;
            }
            offset += info->NextEntryOffset;
        }
    }

    CloseHandle(ioEvent);
}

} // namespace moon
