#include <nativeui/window.hpp>

#include <emscripten.h>

#include <cstdio>
#include <exception>
#include <string_view>

namespace {

ui::Application* owner_application{};
ui::Application* contender_application{};

[[noreturn]] void finish(int code, const char* message) noexcept {
    if (message) {
        std::fprintf(code == 0 ? stdout : stderr, "%s\n", message);
    }
    if (contender_application) {
        delete contender_application;
        contender_application = nullptr;
    }
    if (owner_application) {
        delete owner_application;
        owner_application = nullptr;
    }
    emscripten_force_exit(code);
}

void exercise_contender(void*) noexcept {
    try {
        if (!owner_application || !contender_application) {
            finish(1, "wasm main-loop smoke: missing application state");
        }

        const int contender_result = contender_application->run();
        if (contender_result != 1) {
            finish(1, "wasm main-loop smoke: second run() unexpectedly acquired the module loop");
        }

        const std::string_view error = contender_application->last_error();
        if (error.find("use poll()") == std::string_view::npos) {
            finish(1, "wasm main-loop smoke: run() contention lacked actionable diagnostics");
        }

        delete contender_application;
        contender_application = nullptr;

        // The owner still has an Emscripten callback registered here. Deleting
        // it must cancel that callback before releasing Impl.
        delete owner_application;
        owner_application = nullptr;

        std::puts("wasm main-loop smoke: pass");
        emscripten_force_exit(0);
    } catch (const std::exception& error) {
        finish(1, error.what());
    } catch (...) {
        finish(1, "wasm main-loop smoke: unknown exception");
    }
}

} // namespace

int main() {
    try {
        owner_application = new ui::Application{};
        contender_application = new ui::Application{};
        if (!owner_application->valid() || !contender_application->valid()) {
            finish(1, "wasm main-loop smoke: Application construction failed");
        }

        // run() uses Emscripten's simulated infinite loop and does not return.
        // The async callback executes while the first Application owns the one
        // module-level browser loop.
        emscripten_async_call(&exercise_contender, nullptr, 1);
        const int unexpected = owner_application->run();
        std::fprintf(stderr, "wasm main-loop smoke: owner run() returned %d\n", unexpected);
        finish(1, "wasm main-loop smoke: owner run() unexpectedly returned");
    } catch (const std::exception& error) {
        finish(1, error.what());
    } catch (...) {
        finish(1, "wasm main-loop smoke: unknown top-level exception");
    }
}
