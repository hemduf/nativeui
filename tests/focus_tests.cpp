#include "test_support.hpp"

namespace {

void suite() {
    // Neutral input normalization: regular Tab keeps its modifier state while
    // AppKit BackTab U+0019 always means reverse traversal.
    {
        const auto tab = ui::detail::normalize_tab_key(0x09U, false);
        NUI_CHECK(tab.is_tab);
        NUI_CHECK(!tab.shift);

        const auto shifted_tab = ui::detail::normalize_tab_key(0x09U, true);
        NUI_CHECK(shifted_tab.is_tab);
        NUI_CHECK(shifted_tab.shift);

        const auto backtab = ui::detail::normalize_tab_key(0x19U, false);
        NUI_CHECK(backtab.is_tab);
        NUI_CHECK(backtab.shift);

        const auto unrelated = ui::detail::normalize_tab_key('x', false);
        NUI_CHECK(!unrelated.is_tab);
    }

    ui::State<bool> first{false};
    ui::State<bool> second{false};
    ui::State<bool> third{false};
    ui::UI tree{
        ui::Row{
            ui::Toggle{"First", first},
            ui::Toggle{"Second", second},
            ui::Toggle{"Third", third},
        }.gap(4.0f)
    };

    test::MockPlatform platform;
    tree.resize({720.0f, 100.0f});
    tree.activate(platform);

    tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(first.get());
    NUI_CHECK(!second.get());
    NUI_CHECK(!third.get());

    tree.dispatch(test::key(ui::Key::Tab), platform);
    tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(second.get());

    // Reverse one position: Second -> First.
    tree.dispatch(test::key(ui::Key::Tab, true), platform);
    tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(!first.get());

    // Reverse wraps: First -> Third.
    tree.dispatch(test::key(ui::Key::Tab, true), platform);
    tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(third.get());



    // A trapping active scope enters on its default focus and wraps both Tab directions.
    {
        ui::State<bool> active{true};
        ui::State<bool> first_in_scope{false};
        ui::State<bool> second_in_scope{false};
        ui::State<bool> outside{false};
        ui::UI scoped{
            ui::Column{
                ui::FocusScope{active,
                    ui::Row{
                        ui::Toggle{"Scoped A", first_in_scope},
                        ui::Toggle{"Scoped B", second_in_scope}}
                        .gap(4.0f)}
                    .trap(true)
                    .default_focus(1),
                ui::Toggle{"Outside", outside}}
                .gap(4.0f)
                .padding(0.0f)};

        test::MockPlatform scoped_platform;
        scoped.resize({720.0f, 160.0f});
        scoped.activate(scoped_platform);

        scoped.dispatch(test::key(ui::Key::Space), scoped_platform);
        NUI_CHECK(!first_in_scope.get());
        NUI_CHECK(second_in_scope.get());
        NUI_CHECK(!outside.get());

        // Default second -> Tab wraps to first instead of escaping to Outside.
        scoped.dispatch(test::key(ui::Key::Tab), scoped_platform);
        scoped.dispatch(test::key(ui::Key::Space), scoped_platform);
        NUI_CHECK(first_in_scope.get());
        NUI_CHECK(!outside.get());

        // First -> Shift+Tab wraps back to second at the scope boundary.
        scoped.dispatch(test::key(ui::Key::Tab, true), scoped_platform);
        scoped.dispatch(test::key(ui::Key::Space), scoped_platform);
        NUI_CHECK(!second_in_scope.get());
        NUI_CHECK(!outside.get());

        // Tree lifecycle restarts the active scope and reapplies its default focus.
        scoped.deactivate(scoped_platform);
        scoped.activate(scoped_platform);
        scoped.dispatch(test::key(ui::Key::Space), scoped_platform);
        NUI_CHECK(second_in_scope.get());
        NUI_CHECK(!outside.get());
    }

    // Activating a scope remembers the previous focus; deactivation restores it.
    {
        ui::State<bool> dialog_active{false};
        ui::State<bool> outside{false};
        ui::State<bool> dialog_a{false};
        ui::State<bool> dialog_b{false};
        ui::UI scoped{
            ui::Column{
                ui::Toggle{"Outside", outside},
                ui::FocusScope{dialog_active,
                    ui::Row{
                        ui::Toggle{"Dialog A", dialog_a},
                        ui::Toggle{"Dialog B", dialog_b}}
                        .gap(4.0f)}
                    .trap(true)
                    .default_focus(1)}
                .gap(4.0f)
                .padding(0.0f)};

        test::MockPlatform scoped_platform;
        scoped.resize({720.0f, 160.0f});
        scoped.activate(scoped_platform);

        // Inactive scope is skipped, so Outside owns initial focus.
        scoped.dispatch(test::key(ui::Key::Space), scoped_platform);
        NUI_CHECK(outside.get());
        NUI_CHECK(!dialog_a.get());
        NUI_CHECK(!dialog_b.get());

        dialog_active.set(true);
        scoped.dispatch(test::key(ui::Key::Space), scoped_platform);
        NUI_CHECK(dialog_b.get());

        dialog_active.set(false);
        scoped.dispatch(test::key(ui::Key::Space), scoped_platform);
        NUI_CHECK(!outside.get()); // restored to Outside and toggled again
        NUI_CHECK(dialog_b.get());
    }

    // A deactivated tree must not route stray key events or traversal.
    tree.deactivate(platform);
    tree.dispatch(test::key(ui::Key::Tab), platform);
    tree.dispatch(test::key(ui::Key::Space), platform);
    NUI_CHECK(third.get());
}

} // namespace

int main() { return test::run("focus", &suite); }
