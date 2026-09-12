#pragma once

#if defined(__APPLE__)
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace example {

// Root CTest may execute feature self-tests concurrently. AppKit native-window
// lifecycle tests share the hosted runner's WindowServer and must not overlap;
// dedicated platform workflows already execute these contracts in isolation.
// flock() is process-scoped and is released automatically if a test crashes.
class NativeWindowSelfTestLock final {
public:
    NativeWindowSelfTestLock() noexcept {
#if defined(__APPLE__)
        fd_ = ::open("/tmp/nativeui-macos-window-self-test.lock", O_CREAT | O_RDWR, 0600);
        if (fd_ < 0) return;
        while (::flock(fd_, LOCK_EX) != 0) {
            if (errno == EINTR) continue;
            ::close(fd_);
            fd_ = -1;
            return;
        }
#endif
    }

    ~NativeWindowSelfTestLock() {
#if defined(__APPLE__)
        if (fd_ >= 0) {
            (void)::flock(fd_, LOCK_UN);
            (void)::close(fd_);
        }
#endif
    }

    NativeWindowSelfTestLock(const NativeWindowSelfTestLock&) = delete;
    NativeWindowSelfTestLock& operator=(const NativeWindowSelfTestLock&) = delete;

    [[nodiscard]] bool valid() const noexcept {
#if defined(__APPLE__)
        return fd_ >= 0;
#else
        return true;
#endif
    }

private:
#if defined(__APPLE__)
    int fd_{-1};
#endif
};

} // namespace example
