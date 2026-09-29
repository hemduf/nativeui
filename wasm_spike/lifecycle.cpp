#include <nativeui/nativeui.hpp>

#include <emscripten.h>

#include <exception>
#include <memory>
#include <string>

namespace {

EM_JS(void, publish_lifecycle_result, (const char* result, const char* reason), {
    if (!document.body) {
        return;
    }
    document.body.dataset.nativeuiReady = 'true';
    document.body.dataset.nativeuiTest = UTF8ToString(result);
    const message = UTF8ToString(reason);
    if (message) {
        document.body.dataset.nativeuiReason = message;
    } else {
        delete document.body.dataset.nativeuiReason;
    }
});

struct LifecycleHarness final {
    ui::Application application;
    ui::UI a_ui{ui::Spacer{160.0f, 90.0f}};
    ui::UI b_ui{ui::Spacer{170.0f, 100.0f}};
    std::unique_ptr<ui::StandaloneWindow> a;
    std::unique_ptr<ui::StandaloneWindow> b;
    std::string error;

    bool poll_live(int count) {
        for (int i = 0; i < count; ++i) {
            if (!application.poll(0.0)) {
                error = application.last_error().empty()
                            ? "Application stopped while a window remained live"
                            : std::string{application.last_error()};
                return false;
            }
        }
        return true;
    }

    bool initialize() {
        if (!application.valid()) {
            error = std::string{application.last_error()};
            return false;
        }

        a = std::make_unique<ui::StandaloneWindow>(
            application,
            a_ui,
            ui::WindowDesc{
                .title = "NativeUI wasm lifecycle A",
                .size = {320.0f, 180.0f},
                .resizable = true});
        b = std::make_unique<ui::StandaloneWindow>(
            application,
            b_ui,
            ui::WindowDesc{
                .title = "NativeUI wasm lifecycle B",
                .size = {300.0f, 170.0f},
                .resizable = true});

        if (!a->valid() || !a->native_handle()) {
            error = a->last_error().empty() ? "Window A creation failed"
                                            : std::string{a->last_error()};
            return false;
        }
        if (!b->valid() || !b->native_handle()) {
            error = b->last_error().empty() ? "Window B creation failed"
                                            : std::string{b->last_error()};
            return false;
        }

        if (!poll_live(8)) {
            return false;
        }
        if (!a->last_error().empty()) {
            error = std::string{a->last_error()};
            return false;
        }
        if (!b->last_error().empty()) {
            error = std::string{b->last_error()};
            return false;
        }
        return true;
    }

    bool destroy_first() {
        if (!a || !b) {
            error = "Lifecycle destroy-first called in an invalid state";
            return false;
        }

        a.reset();
        if (application.quit_requested()) {
            error = "Destroying A requested quit while B remained live";
            return false;
        }
        if (!b->set_size({340.0f, 190.0f})) {
            error = "Surviving window B rejected resize";
            return false;
        }
        if (!poll_live(8)) {
            return false;
        }
        if (!b->last_error().empty()) {
            error = std::string{b->last_error()};
            return false;
        }
        return true;
    }

    bool finish() {
        if (a || !b) {
            error = "Lifecycle finish called before the survivor phase";
            return false;
        }

        b.reset();
        if (!application.quit_requested()) {
            error = "Destroying the last window did not request quit";
            return false;
        }
        if (application.poll(0.0)) {
            error = "Application poll succeeded after normal last-window quit";
            return false;
        }
        if (!application.last_error().empty()) {
            error = std::string{application.last_error()};
            return false;
        }
        return true;
    }
};

LifecycleHarness* harness{};

int publish_failure(const char* message) noexcept {
    publish_lifecycle_result("fail", message ? message : "unknown failure");
    return 1;
}

} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE int nativeuiWasmLifecycleDestroyFirst() noexcept {
    try {
        if (!harness) {
            return publish_failure("Lifecycle harness is unavailable");
        }
        if (!harness->destroy_first()) {
            return publish_failure(harness->error.c_str());
        }
        return 0;
    } catch (const std::exception& error) {
        return publish_failure(error.what());
    } catch (...) {
        return publish_failure("Unknown exception while destroying window A");
    }
}

extern "C" EMSCRIPTEN_KEEPALIVE int nativeuiWasmLifecycleFinish() noexcept {
    try {
        if (!harness) {
            return publish_failure("Lifecycle harness is unavailable");
        }
        if (!harness->finish()) {
            return publish_failure(harness->error.c_str());
        }

        delete harness;
        harness = nullptr;
        publish_lifecycle_result("pass", "");
        return 0;
    } catch (const std::exception& error) {
        return publish_failure(error.what());
    } catch (...) {
        return publish_failure("Unknown exception while finishing lifecycle smoke");
    }
}

int main() {
    try {
        harness = new LifecycleHarness{};
        if (!harness->initialize()) {
            const std::string reason =
                harness->error.empty() ? "Lifecycle initialization failed" : harness->error;
            delete harness;
            harness = nullptr;
            return publish_failure(reason.c_str());
        }

        publish_lifecycle_result("pending", "");
        return 0;
    } catch (const std::exception& error) {
        if (harness) {
            delete harness;
            harness = nullptr;
        }
        return publish_failure(error.what());
    } catch (...) {
        if (harness) {
            delete harness;
            harness = nullptr;
        }
        return publish_failure("Unknown lifecycle initialization exception");
    }
}
