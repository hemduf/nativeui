#pragma once

#if defined(_WIN32)

#include <nativeui/desktop_services.hpp>

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlobj_core.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <atomic>
#include <climits>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Uuid.lib")

namespace ui::detail {
namespace windows_desktop_services_detail {

using Microsoft::WRL::ComPtr;

class ComApartment final {
public:
    explicit ComApartment(DWORD mode) noexcept
        : result_(CoInitializeEx(nullptr, mode)),
          initialized_(SUCCEEDED(result_)) {}

    ~ComApartment() {
        if (initialized_) CoUninitialize();
    }

    ComApartment(const ComApartment&) = delete;
    ComApartment& operator=(const ComApartment&) = delete;

    [[nodiscard]] bool initialized() const noexcept { return initialized_; }
    [[nodiscard]] bool usable() const noexcept {
        return initialized_ || result_ == RPC_E_CHANGED_MODE;
    }

private:
    HRESULT result_{E_FAIL};
    bool initialized_{};
};

[[nodiscard]] inline bool utf8_to_wide(const std::string& text,
                                       std::wstring& output) {
    output.clear();
    if (text.empty()) return true;
    if (text.size() > static_cast<std::size_t>(INT_MAX)) return false;

    const int source_size = static_cast<int>(text.size());
    const int required = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), source_size, nullptr, 0);
    if (required <= 0) return false;

    output.resize(static_cast<std::size_t>(required));
    return MultiByteToWideChar(
               CP_UTF8,
               MB_ERR_INVALID_CHARS,
               text.data(),
               source_size,
               output.data(),
               required) == required;
}

struct FilterStorage final {
    std::vector<std::wstring> names;
    std::vector<std::wstring> patterns;
    std::vector<COMDLG_FILTERSPEC> specs;
};

[[nodiscard]] inline bool build_filters(const std::vector<FileFilter>& filters,
                                        FilterStorage& storage) {
    storage = {};
    if (filters.empty()) return true;

    storage.names.reserve(filters.size());
    storage.patterns.reserve(filters.size());
    for (const auto& filter : filters) {
        std::wstring pattern;
        for (const auto& extension : filter.extensions) {
            std::wstring wide_extension;
            if (!utf8_to_wide(extension, wide_extension)) return false;
            if (!pattern.empty()) pattern += L';';
            pattern += L'*';
            pattern += wide_extension;
        }
        if (pattern.empty()) pattern = L"*.*";

        std::wstring name;
        if (!filter.description.empty() && !utf8_to_wide(filter.description, name)) {
            return false;
        }
        if (name.empty()) name = pattern;

        storage.names.push_back(std::move(name));
        storage.patterns.push_back(std::move(pattern));
    }

    storage.specs.reserve(filters.size());
    for (std::size_t index = 0; index < filters.size(); ++index) {
        storage.specs.push_back(COMDLG_FILTERSPEC{
            storage.names[index].c_str(), storage.patterns[index].c_str()});
    }
    return true;
}

[[nodiscard]] inline bool set_title(IFileDialog& dialog, const std::string& title) {
    if (title.empty()) return true;
    std::wstring value;
    return utf8_to_wide(title, value) && SUCCEEDED(dialog.SetTitle(value.c_str()));
}

[[nodiscard]] inline bool set_initial_directory(
    IFileDialog& dialog,
    const std::optional<std::filesystem::path>& directory) {
    if (!directory) return true;
    ComPtr<IShellItem> item;
    const HRESULT result = SHCreateItemFromParsingName(
        directory->c_str(), nullptr, IID_PPV_ARGS(&item));
    return SUCCEEDED(result) && SUCCEEDED(dialog.SetFolder(item.Get()));
}

[[nodiscard]] inline bool set_filters(IFileDialog& dialog,
                                      const std::vector<FileFilter>& filters,
                                      FilterStorage& storage) {
    if (!build_filters(filters, storage)) return false;
    if (storage.specs.empty()) return true;
    return SUCCEEDED(dialog.SetFileTypes(
        static_cast<UINT>(storage.specs.size()), storage.specs.data()));
}

[[nodiscard]] inline bool append_shell_item_path(
    IShellItem& item,
    std::vector<std::filesystem::path>& paths) {
    PWSTR raw_path = nullptr;
    if (FAILED(item.GetDisplayName(SIGDN_FILESYSPATH, &raw_path)) || !raw_path) {
        return false;
    }
    try {
        paths.emplace_back(raw_path);
    } catch (...) {
        CoTaskMemFree(raw_path);
        return false;
    }
    CoTaskMemFree(raw_path);
    return true;
}

[[nodiscard]] inline FileDialogResult error_result(std::string message) {
    FileDialogResult result;
    result.status = DesktopServiceStatus::Error;
    result.error = std::move(message);
    return result;
}

[[nodiscard]] inline FileDialogResult cancelled_result() {
    FileDialogResult result;
    result.status = DesktopServiceStatus::Cancelled;
    return result;
}

struct RequestState final {
    std::atomic<bool> cancel_requested{false};
    std::atomic<bool> finished{false};
    std::mutex marshal_mutex;
    IStream* marshaled_dialog{};
    std::thread worker;
};

[[nodiscard]] inline IStream* take_marshaled_dialog(RequestState& state) noexcept {
    std::lock_guard lock{state.marshal_mutex};
    IStream* stream = state.marshaled_dialog;
    state.marshaled_dialog = nullptr;
    return stream;
}

inline void release_marshaled_dialog_on_worker(RequestState& state) noexcept {
    IStream* stream = take_marshaled_dialog(state);
    if (!stream) return;
    (void)CoReleaseMarshalData(stream);
    stream->Release();
}

inline void request_marshaled_close(const std::shared_ptr<RequestState>& state) noexcept {
    if (!state) return;
    IStream* stream = take_marshaled_dialog(*state);
    if (!stream) return;

    ComApartment apartment{COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE};
    if (!apartment.usable()) {
        std::lock_guard lock{state->marshal_mutex};
        if (!state->marshaled_dialog) {
            state->marshaled_dialog = stream;
            return;
        }
        stream->Release();
        return;
    }

    IFileDialog* dialog = nullptr;
    const HRESULT result = CoGetInterfaceAndReleaseStream(
        stream, IID_IFileDialog, reinterpret_cast<void**>(&dialog));
    if (SUCCEEDED(result) && dialog) {
        (void)dialog->Close(HRESULT_FROM_WIN32(ERROR_CANCELLED));
        dialog->Release();
    }
}

[[nodiscard]] inline bool prepare_marshaled_dialog(
    RequestState& state,
    IFileDialog& dialog) {
    IStream* stream = nullptr;
    if (FAILED(CoMarshalInterThreadInterfaceInStream(
            IID_IFileDialog, &dialog, &stream)) || !stream) {
        return false;
    }
    {
        std::lock_guard lock{state.marshal_mutex};
        state.marshaled_dialog = stream;
    }
    return true;
}

[[nodiscard]] inline FileDialogResult collect_single_result(IFileDialog& dialog) {
    ComPtr<IShellItem> item;
    if (FAILED(dialog.GetResult(&item)) || !item) {
        return error_result("Windows file dialog returned no selected item");
    }

    FileDialogResult result;
    result.status = DesktopServiceStatus::Accepted;
    if (!append_shell_item_path(*item.Get(), result.paths)) {
        return error_result("Windows file dialog returned an invalid filesystem path");
    }
    return result;
}

[[nodiscard]] inline FileDialogResult collect_multiple_results(IFileOpenDialog& dialog) {
    ComPtr<IShellItemArray> items;
    if (FAILED(dialog.GetResults(&items)) || !items) {
        return error_result("Windows file dialog returned no selected items");
    }

    DWORD count = 0;
    if (FAILED(items->GetCount(&count)) || count == 0) {
        return error_result("Windows file dialog returned an empty selection");
    }

    FileDialogResult result;
    result.status = DesktopServiceStatus::Accepted;
    result.paths.reserve(static_cast<std::size_t>(count));
    for (DWORD index = 0; index < count; ++index) {
        ComPtr<IShellItem> item;
        if (FAILED(items->GetItemAt(index, &item)) || !item ||
            !append_shell_item_path(*item.Get(), result.paths)) {
            return error_result("Windows file dialog returned an invalid filesystem path");
        }
    }
    return result;
}

enum class OpenDialogKind {
    SingleFile,
    MultipleFiles,
    Directory,
};

inline void run_open_dialog(std::shared_ptr<RequestState> state,
                            HWND parent,
                            OpenDialogKind kind,
                            OpenFileOptions options,
                            DirectoryOptions directory_options,
                            FileDialogCallback completion) noexcept {
    FileDialogResult result;
    try {
        ComApartment apartment{COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE};
        if (!apartment.initialized()) {
            result = error_result("COM apartment initialization failed for Windows file dialog");
        } else {
            ComPtr<IFileOpenDialog> dialog;
            if (FAILED(CoCreateInstance(
                    CLSID_FileOpenDialog,
                    nullptr,
                    CLSCTX_INPROC_SERVER,
                    IID_PPV_ARGS(&dialog))) || !dialog) {
                result = error_result("IFileOpenDialog creation failed");
            } else {
                DWORD flags = 0;
                if (FAILED(dialog->GetOptions(&flags))) {
                    result = error_result("IFileOpenDialog GetOptions failed");
                } else {
                    flags |= FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM;
                    if (kind == OpenDialogKind::Directory) {
                        flags |= FOS_PICKFOLDERS;
                    } else {
                        flags |= FOS_FILEMUSTEXIST;
                        if (kind == OpenDialogKind::MultipleFiles) flags |= FOS_ALLOWMULTISELECT;
                    }

                    FilterStorage filters;
                    const std::string& title = kind == OpenDialogKind::Directory
                        ? directory_options.title
                        : options.title;
                    const auto& initial_directory = kind == OpenDialogKind::Directory
                        ? directory_options.initial_directory
                        : options.initial_directory;
                    const bool configured =
                        SUCCEEDED(dialog->SetOptions(flags)) &&
                        set_title(*dialog.Get(), title) &&
                        set_initial_directory(*dialog.Get(), initial_directory) &&
                        (kind == OpenDialogKind::Directory ||
                         set_filters(*dialog.Get(), options.filters, filters));

                    if (!configured) {
                        result = error_result("Windows open dialog configuration failed");
                    } else if (!prepare_marshaled_dialog(*state, *dialog.Get())) {
                        result = error_result("Windows file dialog cancellation channel failed");
                    } else if (state->cancel_requested.load(std::memory_order_acquire)) {
                        result = cancelled_result();
                    } else {
                        const HRESULT show_result = dialog->Show(parent);
                        if (state->cancel_requested.load(std::memory_order_acquire) ||
                            show_result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
                            result = cancelled_result();
                        } else if (FAILED(show_result)) {
                            result = error_result("IFileOpenDialog Show failed");
                        } else if (kind == OpenDialogKind::MultipleFiles) {
                            result = collect_multiple_results(*dialog.Get());
                        } else {
                            result = collect_single_result(*dialog.Get());
                        }
                    }
                    release_marshaled_dialog_on_worker(*state);
                }
            }
        }
    } catch (...) {
        release_marshaled_dialog_on_worker(*state);
        result = error_result("Unexpected Windows open dialog failure");
    }

    state->finished.store(true, std::memory_order_release);
    try {
        completion(std::move(result));
    } catch (...) {
    }
}

inline void run_save_dialog(std::shared_ptr<RequestState> state,
                            HWND parent,
                            SaveFileOptions options,
                            FileDialogCallback completion) noexcept {
    FileDialogResult result;
    try {
        ComApartment apartment{COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE};
        if (!apartment.initialized()) {
            result = error_result("COM apartment initialization failed for Windows save dialog");
        } else {
            ComPtr<IFileSaveDialog> dialog;
            if (FAILED(CoCreateInstance(
                    CLSID_FileSaveDialog,
                    nullptr,
                    CLSCTX_INPROC_SERVER,
                    IID_PPV_ARGS(&dialog))) || !dialog) {
                result = error_result("IFileSaveDialog creation failed");
            } else {
                DWORD flags = 0;
                FilterStorage filters;
                bool configured =
                    SUCCEEDED(dialog->GetOptions(&flags)) &&
                    SUCCEEDED(dialog->SetOptions(
                        flags | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT)) &&
                    set_title(*dialog.Get(), options.title) &&
                    set_initial_directory(*dialog.Get(), options.initial_directory) &&
                    set_filters(*dialog.Get(), options.filters, filters);

                std::wstring suggested;
                if (configured && options.suggested_filename) {
                    configured = utf8_to_wide(*options.suggested_filename, suggested) &&
                                 SUCCEEDED(dialog->SetFileName(suggested.c_str()));
                }

                if (!configured) {
                    result = error_result("Windows save dialog configuration failed");
                } else if (!prepare_marshaled_dialog(*state, *dialog.Get())) {
                    result = error_result("Windows save dialog cancellation channel failed");
                } else if (state->cancel_requested.load(std::memory_order_acquire)) {
                    result = cancelled_result();
                } else {
                    const HRESULT show_result = dialog->Show(parent);
                    if (state->cancel_requested.load(std::memory_order_acquire) ||
                        show_result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
                        result = cancelled_result();
                    } else if (FAILED(show_result)) {
                        result = error_result("IFileSaveDialog Show failed");
                    } else {
                        result = collect_single_result(*dialog.Get());
                    }
                }
                release_marshaled_dialog_on_worker(*state);
            }
        }
    } catch (...) {
        release_marshaled_dialog_on_worker(*state);
        result = error_result("Unexpected Windows save dialog failure");
    }

    state->finished.store(true, std::memory_order_release);
    try {
        completion(std::move(result));
    } catch (...) {
    }
}

} // namespace windows_desktop_services_detail

class WindowsDesktopServicesBackend final : public DesktopServicesBackend {
public:
    explicit WindowsDesktopServicesBackend(std::uintptr_t parent_window) noexcept
        : parent_(reinterpret_cast<HWND>(parent_window)) {}

    ~WindowsDesktopServicesBackend() override {
        std::vector<std::shared_ptr<windows_desktop_services_detail::RequestState>> requests;
        {
            std::lock_guard lock{mutex_};
            closing_ = true;
            requests.reserve(active_.size());
            for (const auto& [id, request] : active_) {
                (void)id;
                request->cancel_requested.store(true, std::memory_order_release);
                requests.push_back(request);
            }
            active_.clear();
        }

        for (const auto& request : requests) {
            windows_desktop_services_detail::request_marshaled_close(request);
        }
        for (const auto& request : requests) {
            if (request->worker.joinable()) request->worker.join();
        }
    }

    DesktopServiceStatus start_open_file(DesktopRequestId request_id,
                                         const OpenFileOptions& options,
                                         FileDialogCallback completion) override {
        return start_open_dialog(request_id,
                                 windows_desktop_services_detail::OpenDialogKind::SingleFile,
                                 options,
                                 {},
                                 std::move(completion));
    }

    DesktopServiceStatus start_open_files(DesktopRequestId request_id,
                                          const OpenFileOptions& options,
                                          FileDialogCallback completion) override {
        return start_open_dialog(request_id,
                                 windows_desktop_services_detail::OpenDialogKind::MultipleFiles,
                                 options,
                                 {},
                                 std::move(completion));
    }

    DesktopServiceStatus start_save_file(DesktopRequestId request_id,
                                         const SaveFileOptions& options,
                                         FileDialogCallback completion) override {
        using namespace windows_desktop_services_detail;
        if (request_id == kInvalidDesktopRequestId || !completion) {
            return DesktopServiceStatus::InvalidArgument;
        }
        reap_finished();

        auto state = std::make_shared<RequestState>();
        {
            std::lock_guard lock{mutex_};
            if (closing_ || active_.contains(request_id)) return DesktopServiceStatus::Error;
            active_.emplace(request_id, state);
        }

        try {
            const HWND parent = parent_;
            state->worker = std::thread(
                [state, parent, options, completion = std::move(completion)]() mutable {
                    run_save_dialog(state, parent, options, std::move(completion));
                });
        } catch (...) {
            std::lock_guard lock{mutex_};
            active_.erase(request_id);
            return DesktopServiceStatus::Error;
        }
        return DesktopServiceStatus::Accepted;
    }

    DesktopServiceStatus start_select_directory(DesktopRequestId request_id,
                                                const DirectoryOptions& options,
                                                FileDialogCallback completion) override {
        return start_open_dialog(request_id,
                                 windows_desktop_services_detail::OpenDialogKind::Directory,
                                 {},
                                 options,
                                 std::move(completion));
    }

    DesktopServiceStatus start_open_url(DesktopRequestId request_id,
                                        std::string url,
                                        StatusCallback completion) override {
        if (request_id == kInvalidDesktopRequestId || !completion) {
            return DesktopServiceStatus::InvalidArgument;
        }
        reap_finished();
        {
            std::lock_guard lock{mutex_};
            if (closing_) return DesktopServiceStatus::Error;
        }

        std::wstring wide_url;
        if (!windows_desktop_services_detail::utf8_to_wide(url, wide_url)) {
            return DesktopServiceStatus::Error;
        }
        const auto result = reinterpret_cast<std::intptr_t>(ShellExecuteW(
            parent_, L"open", wide_url.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
        if (result <= 32) return DesktopServiceStatus::Error;

        completion(DesktopServiceStatus::Accepted);
        return DesktopServiceStatus::Accepted;
    }

    bool cancel(DesktopRequestId request_id) override {
        using namespace windows_desktop_services_detail;
        if (request_id == kInvalidDesktopRequestId) return false;
        reap_finished();

        std::shared_ptr<RequestState> request;
        {
            std::lock_guard lock{mutex_};
            const auto found = active_.find(request_id);
            if (closing_ || found == active_.end() ||
                found->second->finished.load(std::memory_order_acquire)) {
                return false;
            }
            request = found->second;
            request->cancel_requested.store(true, std::memory_order_release);
        }
        request_marshaled_close(request);
        return true;
    }

private:
    DesktopServiceStatus start_open_dialog(
        DesktopRequestId request_id,
        windows_desktop_services_detail::OpenDialogKind kind,
        OpenFileOptions options,
        DirectoryOptions directory_options,
        FileDialogCallback completion) {
        using namespace windows_desktop_services_detail;
        if (request_id == kInvalidDesktopRequestId || !completion) {
            return DesktopServiceStatus::InvalidArgument;
        }
        reap_finished();

        auto state = std::make_shared<RequestState>();
        {
            std::lock_guard lock{mutex_};
            if (closing_ || active_.contains(request_id)) return DesktopServiceStatus::Error;
            active_.emplace(request_id, state);
        }

        try {
            const HWND parent = parent_;
            state->worker = std::thread(
                [state,
                 parent,
                 kind,
                 options = std::move(options),
                 directory_options = std::move(directory_options),
                 completion = std::move(completion)]() mutable {
                    run_open_dialog(state,
                                    parent,
                                    kind,
                                    std::move(options),
                                    std::move(directory_options),
                                    std::move(completion));
                });
        } catch (...) {
            std::lock_guard lock{mutex_};
            active_.erase(request_id);
            return DesktopServiceStatus::Error;
        }
        return DesktopServiceStatus::Accepted;
    }

    void reap_finished() {
        using windows_desktop_services_detail::RequestState;
        std::vector<std::shared_ptr<RequestState>> finished;
        {
            std::lock_guard lock{mutex_};
            for (auto iterator = active_.begin(); iterator != active_.end();) {
                if (iterator->second->finished.load(std::memory_order_acquire)) {
                    finished.push_back(iterator->second);
                    iterator = active_.erase(iterator);
                } else {
                    ++iterator;
                }
            }
        }
        for (const auto& request : finished) {
            if (request->worker.joinable()) request->worker.join();
        }
    }

    HWND parent_{};
    std::mutex mutex_;
    std::unordered_map<DesktopRequestId,
                       std::shared_ptr<windows_desktop_services_detail::RequestState>> active_;
    bool closing_{};
};

[[nodiscard]] inline std::shared_ptr<DesktopServicesBackend>
make_windows_desktop_services_backend(std::uintptr_t parent_window) {
    return std::make_shared<WindowsDesktopServicesBackend>(parent_window);
}

} // namespace ui::detail

#endif // defined(_WIN32)
