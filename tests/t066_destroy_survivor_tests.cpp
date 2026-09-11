#include <nativeui/nativeui.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string_view>

namespace {

bool pump(ui::Application& app, int iterations) {
    for (int i = 0; i < iterations; ++i) {
        if (!app.poll(0.0) && !app.quit_requested()) return false;
    }
    return true;
}

int fail(std::string_view message) {
    std::cerr << "FAIL t066_destroy_survivor_tests: " << message << '\n';
    return EXIT_FAILURE;
}

} // namespace

int main() {
    ui::Application app;
    if (!app.valid()) return fail("Application construction failed");

    ui::UI tree_a{ui::Label{"T066 direct-destroy A"}};
    ui::UI tree_b{ui::Label{"T066 direct-destroy B"}};
    auto a = std::make_unique<ui::StandaloneWindow>(
        app,
        tree_a,
        ui::WindowDesc{.title = "T066 direct-destroy A",
                       .size = {420.0f, 220.0f},
                       .resizable = true});
    auto b = std::make_unique<ui::StandaloneWindow>(
        app,
        tree_b,
        ui::WindowDesc{.title = "T066 direct-destroy B",
                       .size = {360.0f, 180.0f},
                       .resizable = true});
    if (!a->valid() || !b->valid()) return fail("window construction failed");
    if (!pump(app, 8)) return fail("Application stopped before direct destruction");

    a.reset();
    if (app.quit_requested()) return fail("destroying A requested quit while B remained live");
    if (!b->native_handle()) return fail("surviving B lost its native handle");
    if (b->should_close()) return fail("surviving B became closing after destroying A");
    if (!b->set_title("T066 direct-destroy B survives")) {
        return fail("surviving B title update failed");
    }
    if (!b->set_size({380.0f, 190.0f})) {
        return fail("surviving B resize failed");
    }
    if (!pump(app, 4)) return fail("Application stopped while B remained live");

    b.reset();
    if (!app.quit_requested()) return fail("destroying final window did not request quit");

    std::cout << "PASS t066_destroy_survivor_tests\n";
    return EXIT_SUCCESS;
}
