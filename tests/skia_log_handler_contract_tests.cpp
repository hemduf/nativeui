#include "include/utils/SkLogHandler.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <utility>

namespace {

class ProbeLogHandler final : public SkLogHandler {
public:
    explicit ProbeLogHandler(std::atomic<unsigned>* callbacks = nullptr) noexcept
        : callbacks_(callbacks) {}

    void onLog(SkLogPriority, const char[], va_list) override {
        if (callbacks_) {
            callbacks_->fetch_add(1U, std::memory_order_relaxed);
        }
    }

private:
    std::atomic<unsigned>* callbacks_{};
};

bool require(bool condition, const char* message) {
    if (condition) {
        return true;
    }
    std::fprintf(stderr, "%s\n", message);
    return false;
}

bool one_process_owner_is_immutable_after_install() {
    if (!require(!SkLogHandler::GetInstance(),
                 "test process must start without a Skia log handler")) {
        return false;
    }

    std::atomic<unsigned> callbacks{0U};
    auto first = sk_make_sp<ProbeLogHandler>(&callbacks);
    auto* first_identity = first.get();

    if (!require(SkLogHandler::SetInstance(first),
                 "first process-wide Skia log handler install must succeed")) {
        return false;
    }

    auto installed = SkLogHandler::GetInstance();
    if (!require(installed.get() == first_identity,
                 "installed handler identity must be retained process-wide")) {
        return false;
    }

    auto second = sk_make_sp<ProbeLogHandler>();
    if (!require(!SkLogHandler::SetInstance(std::move(second)),
                 "a second owner must not replace the installed handler")) {
        return false;
    }

    if (!require(SkLogHandler::GetInstance().get() == first_identity,
                 "failed replacement must preserve the first handler")) {
        return false;
    }

    first.reset();
    installed.reset();

    if (!require(SkLogHandler::GetInstance().get() == first_identity,
                 "dropping local references must not release the process handler")) {
        return false;
    }

    if (!require(!SkLogHandler::SetInstance(nullptr),
                 "the installed process handler must not be clearable")) {
        return false;
    }

    return require(SkLogHandler::GetInstance().get() == first_identity,
                   "clear attempt must preserve the installed handler");
}

bool repeated_lifetime_attempts_cannot_take_ownership() {
    auto* installed = SkLogHandler::GetInstance().get();
    if (!require(installed != nullptr,
                 "precondition: process handler must already be installed")) {
        return false;
    }

    for (unsigned i = 0; i < 256U; ++i) {
        auto candidate = sk_make_sp<ProbeLogHandler>();
        if (!require(!SkLogHandler::SetInstance(std::move(candidate)),
                     "repeated owners must not acquire the process-global slot")) {
            return false;
        }
        if (!require(SkLogHandler::GetInstance().get() == installed,
                     "repeated ownership attempts must preserve handler identity")) {
            return false;
        }
    }

    return true;
}

} // namespace

int main() {
    if (!one_process_owner_is_immutable_after_install()) {
        return 1;
    }
    if (!repeated_lifetime_attempts_cannot_take_ownership()) {
        return 1;
    }
    return 0;
}
