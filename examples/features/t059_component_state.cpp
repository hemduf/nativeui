#include "example_support.hpp"

namespace {

struct DemoState {
    ui::State<bool> shown{true};
    ui::State<bool> in_layout{true};
    ui::State<bool> enabled{true};
    ui::State<bool> read_only{false};
    ui::State<std::string> text{"NativeUI component state"};
};

ui::UI make_ui(DemoState& state) {
    return ui::UI{
        ui::Column{
            ui::Header{"T059 — component availability"},
            ui::Row{
                ui::Toggle{"Shown", state.shown},
                ui::Toggle{"In layout", state.in_layout},
                ui::Toggle{"Enabled", state.enabled},
                ui::Toggle{"Read only", state.read_only}
            }.gap(12.0f),
            ui::Visibility{
                state.shown,
                ui::Visibility{
                    state.in_layout,
                    ui::Enabled{
                        state.enabled,
                        ui::ReadOnly{
                            state.read_only,
                            ui::TextInput{"Value", state.text}
                                .placeholder("Try keyboard, pointer and clipboard input")
                        }
                    }
                }.mode(ui::VisibilityMode::Collapsed)
            }.mode(ui::VisibilityMode::Hidden),
            ui::Label{
                "Shown=false keeps layout but suppresses paint/input/focus. "
                "In layout=false collapses the retained subtree."
            }.size(12.0f).color(ui::colors::textMuted)
        }.gap(14.0f)
    };
}

int self_test() {
    DemoState state;
    auto tree = make_ui(state);
    ui::HeadlessRenderer renderer{{760.0f, 320.0f}, 1.0f};

    const auto visible = tree.measure();
    if (!(visible.preferred.w > 0.0f && visible.preferred.h > 0.0f)) {
        return example::fail("visible tree must have non-zero preferred size");
    }
    if (!renderer.render(tree) || tree.dirty()) {
        return example::fail("initial headless render must consume dirty state");
    }

    state.shown.set(false);
    if (tree.layout_dirty() || !tree.paint_dirty()) {
        return example::fail("Hidden must repaint without invalidating layout");
    }
    const auto hidden = tree.measure();
    if (!example::near(hidden.preferred.w, visible.preferred.w) ||
        !example::near(hidden.preferred.h, visible.preferred.h)) {
        return example::fail("Hidden must preserve layout contribution");
    }
    if (!renderer.render(tree)) return example::fail("Hidden render failed");

    state.shown.set(true);
    if (!renderer.render(tree)) return example::fail("Visible restore render failed");
    state.in_layout.set(false);
    if (!tree.layout_dirty()) {
        return example::fail("Collapsed must invalidate layout");
    }
    const auto collapsed = tree.measure();
    if (!(collapsed.preferred.h < visible.preferred.h)) {
        return example::fail("Collapsed subtree must reduce parent preferred extent");
    }
    if (!renderer.render(tree)) return example::fail("Collapsed render failed");

    state.in_layout.set(true);
    if (!renderer.render(tree)) return example::fail("Collapsed restore render failed");
    state.enabled.set(false);
    if (tree.layout_dirty() || !tree.paint_dirty()) {
        return example::fail("Disabled must repaint without relayout");
    }
    if (!renderer.render(tree)) return example::fail("Disabled render failed");

    state.read_only.set(true);
    if (tree.layout_dirty() || !tree.paint_dirty()) {
        return example::fail("ReadOnly must repaint without relayout");
    }
    if (!renderer.render(tree)) return example::fail("ReadOnly render failed");

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    DemoState state;
    auto tree = make_ui(state);
    return example::run_window(tree, "NativeUI T059 Component State", {760.0f, 320.0f});
}
