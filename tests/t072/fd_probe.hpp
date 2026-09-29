#pragma once

#include <cstddef>
#include <dirent.h>

namespace t072_test {

/// Number of open file descriptors of the calling process, or
/// `static_cast<std::size_t>(-1)` when no descriptor directory is readable.
/// Linux uses /proc/self/fd and the local macOS development probe falls back to
/// /dev/fd; the value is only used for before/after leak comparisons.
[[nodiscard]] inline std::size_t open_file_descriptor_count() noexcept {
    static constexpr const char* kCandidateDirectories[] = {"/proc/self/fd", "/dev/fd"};
    for (const char* path : kCandidateDirectories) {
        DIR* directory = opendir(path);
        if (directory == nullptr) {
            continue;
        }
        std::size_t count = 0;
        for (dirent* entry = readdir(directory); entry != nullptr; entry = readdir(directory)) {
            ++count;
        }
        closedir(directory);
        return count;
    }
    return static_cast<std::size_t>(-1);
}

} // namespace t072_test
