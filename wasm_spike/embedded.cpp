#include <nativeui/nativeui.hpp>

#include <emscripten.h>

#include <cstdint>
#include <exception>
#include <memory>
#include <string>

namespace {

constexpr std::uintptr_t kHostA = 10001U;
constexpr std::uintptr_t kHostB = 10002U;

EM_JS(int, create_embed_hosts, (std::uintptr_t a, std::uintptr_t b), {
    if (!document.body) {
        return 0;
    }

    const create = (token, left) => {
        const host = document.createElement('div');
        host.dataset.puglNativeView = String(token);
        host.dataset.nativeuiEmbedHost = String(token);
        host.style.position = 'fixed';
        host.style.left = left + 'px';
        host.style.top = '24px';
        host.style.width = '260px';
        host.style.height = '150px';
        host.style.overflow = 'hidden';
        host.style.background = '#111';
        document.body.appendChild(host);
        return host;
    };

    return create(a, 24) && create(b, 320) ? 1 : 0;
});

EM_JS(void, remove_embed_hosts, (), {
    document
      .querySelectorAll('[data-nativeui-embed-host]')
      .forEach((element) => element.remove());
});

EM_JS(void, publish_embedded_result, (const char* result, const char* reason), {
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

struct EmbeddedHarness final {
    ui::UI a_ui{ui::Spacer{160.0f, 90.0f}};
    ui::UI b_ui{ui::Spacer{170.0f, 100.0f}};
    std::unique_ptr<ui::EmbeddedView> a;
    std::unique_ptr<ui::EmbeddedView> b;
    std::string error;

    bool poll_view(ui::EmbeddedView& view, int count) {
        for (int i = 0; i < count; ++i) {
            if (!view.poll() && !view.should_close()) {
                error = view.last_error().empty()
                            ? "Embedded view stopped unexpectedly"
                            : std::string{view.last_error()};
                return false;
            }
        }
        return true;
    }

    bool initialize() {
        if (!create_embed_hosts(kHostA, kHostB)) {
            error = "Failed to create browser embed hosts";
            return false;
        }

        a = std::make_unique<ui::EmbeddedView>(
            a_ui, static_cast<ui::NativeParentHandle>(kHostA), ui::Size{220.0f, 120.0f});
        b = std::make_unique<ui::EmbeddedView>(
            b_ui, static_cast<ui::NativeParentHandle>(kHostB), ui::Size{230.0f, 125.0f});

        if (!a->native_handle()) {
            error = a->last_error().empty() ? "Embedded A creation failed"
                                            : std::string{a->last_error()};
            return false;
        }
        if (!b->native_handle()) {
            error = b->last_error().empty() ? "Embedded B creation failed"
                                            : std::string{b->last_error()};
            return false;
        }

        if (!poll_view(*a, 8) || !poll_view(*b, 8)) {
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
            error = "Embedded destroy-first called in an invalid state";
            return false;
        }

        a.reset();
        if (!b->set_size({245.0f, 135.0f})) {
            error = "Surviving embedded B rejected resize";
            return false;
        }
        if (!poll_view(*b, 8)) {
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
            error = "Embedded finish called before the survivor phase";
            return false;
        }

        b.reset();
        remove_embed_hosts();
        return true;
    }

    ~EmbeddedHarness() {
        a.reset();
        b.reset();
        remove_embed_hosts();
    }
};

EmbeddedHarness* harness{};

int publish_failure(const char* message) noexcept {
    publish_embedded_result("fail", message ? message : "unknown failure");
    return 1;
}

} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE int nativeuiWasmEmbeddedDestroyFirst() noexcept {
    try {
        if (!harness) {
            return publish_failure("Embedded harness is unavailable");
        }
        if (!harness->destroy_first()) {
            return publish_failure(harness->error.c_str());
        }
        return 0;
    } catch (const std::exception& error) {
        return publish_failure(error.what());
    } catch (...) {
        return publish_failure("Unknown exception while destroying embedded A");
    }
}

extern "C" EMSCRIPTEN_KEEPALIVE int nativeuiWasmEmbeddedFinish() noexcept {
    try {
        if (!harness) {
            return publish_failure("Embedded harness is unavailable");
        }
        if (!harness->finish()) {
            return publish_failure(harness->error.c_str());
        }

        delete harness;
        harness = nullptr;
        publish_embedded_result("pass", "");
        return 0;
    } catch (const std::exception& error) {
        return publish_failure(error.what());
    } catch (...) {
        return publish_failure("Unknown exception while finishing embedded smoke");
    }
}

int main() {
    try {
        harness = new EmbeddedHarness{};
        if (!harness->initialize()) {
            const std::string reason =
                harness->error.empty() ? "Embedded initialization failed" : harness->error;
            delete harness;
            harness = nullptr;
            return publish_failure(reason.c_str());
        }

        publish_embedded_result("pending", "");
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
        return publish_failure("Unknown embedded initialization exception");
    }
}
