#include <nativeui/nativeui.hpp>

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string_view>

#if defined(__linux__)
#include <execinfo.h>
#include <unistd.h>
#endif

namespace {
int fail(int code, std::string_view stage, std::string_view message) {
    std::cerr << "[t048 embedded] " << stage << ": " << message << " (code " << code << ")\n";
    return code;
}

void trace(std::string_view stage) {
    std::cerr << "[t048 embedded] " << stage << '\n' << std::flush;
}

#if defined(__linux__)
void crash_handler(int signal) {
    static constexpr char prefix[] = "[t048 embedded] fatal signal; native backtrace follows\n";
    (void)::write(STDERR_FILENO, prefix, sizeof(prefix) - 1U);
    void* frames[64]{};
    const int count = ::backtrace(frames, 64);
    ::backtrace_symbols_fd(frames, count, STDERR_FILENO);
    std::_Exit(128 + signal);
}

void install_crash_handler() {
    std::signal(SIGSEGV, crash_handler);
    std::signal(SIGABRT, crash_handler);
    std::signal(SIGBUS, crash_handler);
}
#else
void install_crash_handler() {}
#endif
}

int main(int argc, char** argv) {
    install_crash_handler();
    const char* stage = "arguments";
    try {
        if (argc != 2 || std::string_view{argv[1]} != "--self-test") {
            return fail(2, stage, "expected --self-test");
        }

        stage = "construct-parent-ui";
        ui::UI parent_ui{
            ui::Column{
                ui::Header{"T048 embedded host seam"},
                ui::Spacer{80.0f},
            }.padding(8.0f).gap(4.0f)};

        stage = "construct-parent";
        trace(stage);
        ui::StandaloneWindow parent{
            parent_ui,
            ui::WindowDesc{.title = "T048 embedded host seam",
                           .size = {300.0f, 180.0f},
                           .resizable = true}};
        const ui::NativeParentHandle host_parent = parent.native_handle();
        if (!host_parent) return fail(3, stage, "native parent handle is zero");

        stage = "construct-child-ui";
        ui::State<bool> enabled{true};
        ui::UI child_ui{
            ui::Column{
                ui::Header{"T048 embedded consumer"},
                ui::Toggle{"Enabled", enabled},
            }.padding(8.0f).gap(4.0f)};

        stage = "construct-child";
        trace(stage);
        {
            ui::EmbeddedView child{child_ui, host_parent, {220.0f, 100.0f}};
            if (!child.native_handle()) return fail(4, stage, "embedded native handle is zero");
            if (!(child.scale_factor() > 0.0f)) return fail(4, stage, "invalid scale factor");

            stage = "poll";
            trace(stage);
            const auto start = std::chrono::steady_clock::now();
            for (int i = 0; i < 64; ++i) {
                (void)parent.poll(0.0);
                (void)child.poll();
            }
            if (std::chrono::steady_clock::now() - start > std::chrono::seconds(1)) {
                return fail(5, stage, "non-blocking poll loop exceeded one second");
            }
            if (!parent.last_error().empty()) return fail(6, stage, parent.last_error());
            if (!child.last_error().empty()) return fail(6, stage, child.last_error());

            stage = "resize";
            trace(stage);
            if (!child.set_size({230.0f, 110.0f})) return fail(7, stage, "set_size failed");
            child.request_close();
            if (!child.should_close()) return fail(8, stage, "request_close did not update state");

            stage = "destroy-child";
            trace(stage);
        }
        trace("child-destroyed");

        stage = "close-parent";
        parent.request_close();
        if (!parent.should_close()) return fail(9, stage, "parent request_close did not update state");
        trace("complete");
        return 0;
    } catch (const std::exception& e) {
        return fail(10, stage, e.what());
    } catch (...) {
        return fail(11, stage, "unknown exception");
    }
}
