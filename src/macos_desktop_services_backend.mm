#include "detail/macos_desktop_services.hpp"

#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <filesystem>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ui::detail {
namespace {

[[nodiscard]] NSString* utf8_string(const std::string& value) {
    return [[[NSString alloc] initWithBytes:value.data()
                                     length:value.size()
                                   encoding:NSUTF8StringEncoding] autorelease];
}

[[nodiscard]] NSURL* directory_url(const std::optional<std::filesystem::path>& path) {
    if (!path) return nil;
    const auto& native = path->native();
    NSString* string = [[NSFileManager defaultManager]
        stringWithFileSystemRepresentation:native.c_str()
                                    length:native.size()];
    return string ? [NSURL fileURLWithPath:string isDirectory:YES] : nil;
}

[[nodiscard]] bool configure_filters(NSSavePanel* panel,
                                     const std::vector<FileFilter>& filters) {
    if (filters.empty()) return true;

    NSMutableArray<UTType*>* content_types = [NSMutableArray array];
    for (const auto& filter : filters) {
        for (const auto& extension : filter.extensions) {
            if (extension.size() < 2 || extension.front() != '.') return false;
            NSString* value = utf8_string(extension.substr(1));
            if (!value) return false;
            UTType* type = [UTType typeWithFilenameExtension:value];
            if (!type) return false;
            [content_types addObject:type];
        }
    }

    if ([content_types count] != 0) {
        [panel setAllowedContentTypes:content_types];
    }
    return true;
}

[[nodiscard]] bool append_url_path(NSURL* url,
                                   std::vector<std::filesystem::path>& paths) {
    if (!url || ![url isFileURL]) return false;
    const char* representation = [url fileSystemRepresentation];
    if (!representation) return false;
    try {
        paths.emplace_back(representation);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

class MacDesktopServicesBackend final
    : public DesktopServicesBackend,
      public std::enable_shared_from_this<MacDesktopServicesBackend> {
public:
    explicit MacDesktopServicesBackend(NativeViewHandle parent_view)
        : parent_view_(parent_view) {}

    ~MacDesktopServicesBackend() override {
        std::vector<NSSavePanel*> panels;
        {
            std::lock_guard lock{mutex_};
            closing_ = true;
            panels.reserve(active_.size());
            for (auto& [id, entry] : active_) {
                (void)id;
                if (entry.panel) panels.push_back(entry.panel);
            }
            active_.clear();
        }

        for (NSSavePanel* panel : panels) {
            [panel cancel:nil];
            [panel orderOut:nil];
            [panel release];
        }
    }

    DesktopServiceStatus start_open_file(DesktopRequestId request_id,
                                         const OpenFileOptions& options,
                                         FileDialogCallback completion) override {
        return start_open_panel(request_id, options, false, std::move(completion));
    }

    DesktopServiceStatus start_open_files(DesktopRequestId request_id,
                                          const OpenFileOptions& options,
                                          FileDialogCallback completion) override {
        return start_open_panel(request_id, options, true, std::move(completion));
    }

    DesktopServiceStatus start_save_file(DesktopRequestId request_id,
                                         const SaveFileOptions& options,
                                         FileDialogCallback completion) override {
        @autoreleasepool {
            if (request_id == kInvalidDesktopRequestId || !completion) {
                return DesktopServiceStatus::InvalidArgument;
            }

            NSSavePanel* panel = [NSSavePanel savePanel];
            if (!panel || !configure_common(panel, options.title,
                                            options.initial_directory,
                                            options.filters)) {
                return DesktopServiceStatus::Error;
            }
            if (options.suggested_filename) {
                NSString* filename = utf8_string(*options.suggested_filename);
                if (!filename) return DesktopServiceStatus::Error;
                [panel setNameFieldStringValue:filename];
            }
            return begin_panel(request_id, panel, false, std::move(completion));
        }
    }

    DesktopServiceStatus start_select_directory(DesktopRequestId request_id,
                                                const DirectoryOptions& options,
                                                FileDialogCallback completion) override {
        @autoreleasepool {
            if (request_id == kInvalidDesktopRequestId || !completion) {
                return DesktopServiceStatus::InvalidArgument;
            }

            NSOpenPanel* panel = [NSOpenPanel openPanel];
            if (!panel || !configure_common(panel, options.title,
                                            options.initial_directory, {})) {
                return DesktopServiceStatus::Error;
            }
            [panel setCanChooseFiles:NO];
            [panel setCanChooseDirectories:YES];
            [panel setAllowsMultipleSelection:NO];
            return begin_panel(request_id, panel, false, std::move(completion));
        }
    }

    DesktopServiceStatus start_open_url(DesktopRequestId request_id,
                                        std::string url,
                                        StatusCallback completion) override {
        @autoreleasepool {
            if (request_id == kInvalidDesktopRequestId || !completion) {
                return DesktopServiceStatus::InvalidArgument;
            }
            {
                std::lock_guard lock{mutex_};
                if (closing_) return DesktopServiceStatus::Error;
            }

            NSString* text = utf8_string(url);
            if (!text) return DesktopServiceStatus::Error;
            NSURL* native_url = [NSURL URLWithString:text];
            if (!native_url) return DesktopServiceStatus::Error;
            if (![[NSWorkspace sharedWorkspace] openURL:native_url]) {
                return DesktopServiceStatus::Error;
            }
            completion(DesktopServiceStatus::Accepted);
            return DesktopServiceStatus::Accepted;
        }
    }

    bool cancel(DesktopRequestId request_id) override {
        if (request_id == kInvalidDesktopRequestId) return false;

        auto entry = take(request_id);
        if (!entry) return false;

        NSSavePanel* panel = entry->panel;
        [panel cancel:nil];
        [panel orderOut:nil];
        [panel release];

        FileDialogResult result;
        result.status = DesktopServiceStatus::Cancelled;
        entry->completion(std::move(result));
        return true;
    }

private:
    struct Entry final {
        NSSavePanel* panel{};
        FileDialogCallback completion;
        bool multiple{};
    };

    [[nodiscard]] NSWindow* parent_window() const noexcept {
        if (parent_view_ == 0) return nil;
        NSView* view = reinterpret_cast<NSView*>(parent_view_);
        return [view window];
    }

    [[nodiscard]] bool configure_common(
        NSSavePanel* panel,
        const std::string& title,
        const std::optional<std::filesystem::path>& initial_directory,
        const std::vector<FileFilter>& filters) {
        NSString* native_title = utf8_string(title);
        if (!native_title && !title.empty()) return false;
        [panel setTitle:native_title ?: @""];

        if (initial_directory) {
            NSURL* url = directory_url(initial_directory);
            if (!url) return false;
            [panel setDirectoryURL:url];
        }
        return configure_filters(panel, filters);
    }

    DesktopServiceStatus start_open_panel(DesktopRequestId request_id,
                                          const OpenFileOptions& options,
                                          bool multiple,
                                          FileDialogCallback completion) {
        @autoreleasepool {
            if (request_id == kInvalidDesktopRequestId || !completion) {
                return DesktopServiceStatus::InvalidArgument;
            }

            NSOpenPanel* panel = [NSOpenPanel openPanel];
            if (!panel || !configure_common(panel, options.title,
                                            options.initial_directory,
                                            options.filters)) {
                return DesktopServiceStatus::Error;
            }
            [panel setCanChooseFiles:YES];
            [panel setCanChooseDirectories:NO];
            [panel setAllowsMultipleSelection:multiple ? YES : NO];
            return begin_panel(request_id, panel, multiple, std::move(completion));
        }
    }

    DesktopServiceStatus begin_panel(DesktopRequestId request_id,
                                     NSSavePanel* panel,
                                     bool multiple,
                                     FileDialogCallback completion) {
        [panel retain];
        {
            std::lock_guard lock{mutex_};
            if (closing_ || active_.contains(request_id)) {
                [panel release];
                return DesktopServiceStatus::Error;
            }
            active_.emplace(
                request_id,
                Entry{panel, std::move(completion), multiple});
        }

        const std::weak_ptr<MacDesktopServicesBackend> weak = weak_from_this();
        void (^handler)(NSModalResponse) = ^(NSModalResponse response) {
            if (const auto self = weak.lock()) {
                self->finish_panel(request_id, response);
            }
        };

        if (NSWindow* parent = parent_window()) {
            [panel beginSheetModalForWindow:parent completionHandler:handler];
        } else {
            [panel beginWithCompletionHandler:handler];
        }
        return DesktopServiceStatus::Accepted;
    }

    [[nodiscard]] std::optional<Entry> take(DesktopRequestId request_id) {
        std::lock_guard lock{mutex_};
        const auto found = active_.find(request_id);
        if (found == active_.end()) return std::nullopt;
        Entry entry = std::move(found->second);
        active_.erase(found);
        return entry;
    }

    void finish_panel(DesktopRequestId request_id, NSModalResponse response) {
        auto entry = take(request_id);
        if (!entry) return;

        FileDialogResult result;
        if (response != NSModalResponseOK) {
            result.status = DesktopServiceStatus::Cancelled;
        } else {
            result.status = DesktopServiceStatus::Accepted;
            bool valid = true;
            if (NSOpenPanel* open_panel = [entry->panel isKindOfClass:[NSOpenPanel class]]
                    ? static_cast<NSOpenPanel*>(entry->panel)
                    : nil) {
                NSArray<NSURL*>* urls = [open_panel URLs];
                if (entry->multiple) {
                    for (NSURL* url in urls) {
                        if (!append_url_path(url, result.paths)) {
                            valid = false;
                            break;
                        }
                    }
                    valid = valid && !result.paths.empty();
                } else {
                    valid = [urls count] == 1 &&
                            append_url_path([urls firstObject], result.paths);
                }
            } else {
                valid = append_url_path([entry->panel URL], result.paths);
            }

            if (!valid) {
                result.status = DesktopServiceStatus::Error;
                result.paths.clear();
                result.error = "macOS file panel returned an invalid selection";
            }
        }

        NSSavePanel* panel = entry->panel;
        auto completion = std::move(entry->completion);
        [panel release];
        completion(std::move(result));
    }

    NativeViewHandle parent_view_{};
    std::mutex mutex_;
    std::unordered_map<DesktopRequestId, Entry> active_;
    bool closing_{};
};

std::shared_ptr<DesktopServicesBackend>
make_macos_desktop_services_backend(NativeViewHandle parent_view) {
    return std::make_shared<MacDesktopServicesBackend>(parent_view);
}

} // namespace ui::detail
