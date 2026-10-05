#include "example_support.hpp"
#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/toast.hpp>

#include <chrono>

namespace {
using namespace std::chrono_literals;
int self_test() {
    auto clock = std::make_shared<ui::detail::ManualDispatcherClock>();
    ui::detail::DispatcherOwner dispatcher{{}, clock};
    example::Platform platform;
    ui::UI tree{ui::Button{"Document", [] {}}};
    tree.resize({360.0f, 240.0f});
    tree.activate(platform);
    ui::Toast notices{tree, dispatcher.dispatcher()};
    auto first = notices.show({.message = "Document enregistré"});
    auto replacement = notices.show({.message = "Document enregistré", .duration = 1000ms});
    if (!first.handle.valid() && replacement.status == ui::ToastShowStatus::Shown &&
        replacement.handle.valid() && tree.overlay_entries().size() == 1) {
        ui::HeadlessRenderer renderer{{360.0f, 240.0f}, 1.0f};
        if (!renderer.render(tree))
            return example::fail("toast frame did not render");
        const auto bounds = tree.overlay_entries().front().bounds;
        if (!example::near(bounds.y + bounds.h, 240.0f) || bounds.h >= 240.0f)
            return example::fail("toast did not occupy its natural bottom bounds");
    } else {
        return example::fail("message replacement did not retire the previous handle");
    }
    clock->advance(999ms);
    (void)dispatcher.checkpoint();
    if (!replacement.handle.valid())
        return example::fail("toast expired too early");
    clock->advance(1ms);
    (void)dispatcher.checkpoint();
    if (replacement.handle.valid() || dispatcher.active_timer_count() != 0)
        return example::fail("toast expiration retained a message or timer");
    int undo_count = 0;
    auto actionable =
        notices.show({.message = "Élément supprimé", .action_label = "Annuler", .action = [&] {
                          ++undo_count;
                      }});
    tree.resize({360.0f, 240.0f});
    ui::HeadlessRenderer renderer{{360.0f, 240.0f}, 1.0f};
    if (!renderer.render(tree))
        return example::fail("action frame did not render");
    tree.dispatch(example::key(ui::Key::Tab), platform);
    tree.dispatch(example::key(ui::Key::Enter), platform);
    if (undo_count != 1 || actionable.handle.valid() || dispatcher.active_timer_count() != 0)
        return example::fail("toast action did not retire exactly once");
    auto pending = notices.show({.message = "Nouvelle notification"});
    tree.deactivate(platform);
    if (pending.handle.valid() || dispatcher.active_timer_count() != 0)
        return example::fail("deactivation retained a notification");
    return 0;
}
} // namespace
int main(int argc, char **argv) {
    if (example::self_test_requested(argc, argv))
        return self_test();
#ifdef NATIVEUI_EXAMPLE_SELF_TEST_ONLY
    return example::fail("window mode is disabled in self-test-only validation builds");
#else
    ui::Toast *notices = nullptr;
    int undo_count = 0;
    ui::UI tree{ui::Padding{
        24.0f, ui::Column{ui::Label{"Notifications temporaires sans transfert de focus"},
                          ui::Button{"Enregistrer",
                                     [&] {
                                         if (notices)
                                             (void)notices->show(
                                                 {.message = "Document enregistré"});
                                     }},
                          ui::Button{"Supprimer",
                                     [&] {
                                         if (notices)
                                             (void)notices->show({.message = "Élément supprimé",
                                                                  .action_label = "Annuler",
                                                                  .action = [&] { ++undo_count; }});
                                     }}}
                   .gap(12.0f)}};
    ui::Application application;
    ui::StandaloneWindow window{application, tree,
                                ui::WindowDesc{.title = "NativeUI Toast",
                                               .size = {520.0f, 360.0f},
                                               .resizable = true,
                                               .min_size = std::nullopt,
                                               .max_size = std::nullopt}};
    ui::Toast controller{tree, window.dispatcher()};
    notices = &controller;
    const auto result = application.run();
    notices = nullptr;
    return result;
#endif
}
