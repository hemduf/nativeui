#include "example_support.hpp"

namespace {

template <class Factory>
int verify_disabled_transition(
    std::string_view name,
    Factory&& factory,
    bool expect_layout,
    ui::Size size = {480.0f, 220.0f}) {
    ui::State<bool> enabled{true};
    ui::UI tree{ui::Enabled{enabled, factory()}};
    example::Platform platform;
    tree.resize(size);
    tree.activate(platform);
    ui::HeadlessRenderer renderer{size, 1.0f};
    if (!renderer.render(tree)) {
        return example::fail(std::string{name} + " baseline render failed");
    }
    if (tree.layout_dirty() || tree.paint_dirty()) {
        return example::fail(std::string{name} + " baseline did not settle");
    }

    enabled.set(false);
    if (expect_layout) {
        if (!tree.layout_dirty() || !tree.paint_dirty()) {
            return example::fail(
                std::string{name} + " geometry-changing disabled style missed layout + paint");
        }
    } else if (tree.layout_dirty() || !tree.paint_dirty()) {
        return example::fail(
            std::string{name} + " paint-only disabled style dirtied layout or missed paint");
    }
    return 0;
}

int text_contracts() {
    {
        ui::State<std::string> value{"single line"};
        ui::TextInputStyle style;
        style.disabled.control_height = 118.0f;
        if (const int result = verify_disabled_transition(
                "TextInput layout",
                [&] { return ui::TextInput{"Input", value}.style(style); },
                true);
            result != 0) {
            return result;
        }
    }
    {
        ui::State<std::string> value{"single line"};
        ui::TextInputStyle style;
        style.disabled.field_fill = ui::Color{0.12f, 0.18f, 0.24f, 1.0f};
        if (const int result = verify_disabled_transition(
                "TextInput paint",
                [&] { return ui::TextInput{"Input", value}.style(style); },
                false);
            result != 0) {
            return result;
        }
    }
    {
        ui::State<std::string> value{"multi\nline"};
        ui::TextAreaStyle style;
        style.disabled.control_height = 260.0f;
        if (const int result = verify_disabled_transition(
                "TextArea layout",
                [&] { return ui::TextArea{"Area", value}.style(style); },
                true,
                {480.0f, 300.0f});
            result != 0) {
            return result;
        }
    }
    {
        ui::State<std::string> value{"multi\nline"};
        ui::TextAreaStyle style;
        style.disabled.field_fill = ui::Color{0.12f, 0.18f, 0.24f, 1.0f};
        if (const int result = verify_disabled_transition(
                "TextArea paint",
                [&] { return ui::TextArea{"Area", value}.style(style); },
                false,
                {480.0f, 300.0f});
            result != 0) {
            return result;
        }
    }
    return 0;
}

int scrollbar_contracts() {
    {
        ui::ScrollState scroll{ui::ScrollAxis::Vertical};
        ui::ScrollbarStyle style;
        style.disabled.thickness = 18.0f;
        if (const int result = verify_disabled_transition(
                "Scrollbar layout",
                [&] { return ui::ScrollView{scroll, ui::Spacer{160.0f, 640.0f}}.style(style); },
                true,
                {240.0f, 180.0f});
            result != 0) {
            return result;
        }
    }
    {
        ui::ScrollState scroll{ui::ScrollAxis::Vertical};
        ui::ScrollbarStyle style;
        style.disabled.thumb = ui::Color{0.20f, 0.28f, 0.38f, 1.0f};
        if (const int result = verify_disabled_transition(
                "Scrollbar paint",
                [&] { return ui::ScrollView{scroll, ui::Spacer{160.0f, 640.0f}}.style(style); },
                false,
                {240.0f, 180.0f});
            result != 0) {
            return result;
        }
    }
    return 0;
}

int popup_contracts() {
    {
        ui::State<int> selected{1};
        ui::ComboBoxStyle style;
        style.disabled.control_height = 82.0f;
        if (const int result = verify_disabled_transition(
                "ComboBox layout",
                [&] {
                    return ui::ComboBox<int>{selected, {{1, "One", true}, {2, "Two", true}}}
                        .style(style);
                },
                true);
            result != 0) {
            return result;
        }
    }
    {
        ui::State<int> selected{1};
        ui::ComboBoxStyle style;
        style.disabled.fill = ui::Color{0.10f, 0.16f, 0.22f, 1.0f};
        if (const int result = verify_disabled_transition(
                "ComboBox paint",
                [&] {
                    return ui::ComboBox<int>{selected, {{1, "One", true}, {2, "Two", true}}}
                        .style(style);
                },
                false);
            result != 0) {
            return result;
        }
    }
    {
        ui::ComboBoxStyle style;
        style.disabled.control_height = 82.0f;
        if (const int result = verify_disabled_transition(
                "PopupMenu layout",
                [&] {
                    return ui::PopupMenu{
                        "Menu",
                        {ui::PopupMenuItem::action("Action", [] {})}}
                        .style(style);
                },
                true);
            result != 0) {
            return result;
        }
    }
    {
        ui::ComboBoxStyle style;
        style.disabled.fill = ui::Color{0.10f, 0.16f, 0.22f, 1.0f};
        if (const int result = verify_disabled_transition(
                "PopupMenu paint",
                [&] {
                    return ui::PopupMenu{
                        "Menu",
                        {ui::PopupMenuItem::action("Action", [] {})}}
                        .style(style);
                },
                false);
            result != 0) {
            return result;
        }
    }
    return 0;
}

int tabs_contracts() {
    {
        ui::State<int> selected{1};
        ui::TabsStyle style;
        style.disabled.header_height = 74.0f;
        if (const int result = verify_disabled_transition(
                "Tabs layout",
                [&] {
                    return ui::Tabs<int>{selected}
                        .tab(1, "One", ui::Spacer{280.0f, 80.0f})
                        .tab(2, "Two", ui::Spacer{280.0f, 80.0f})
                        .style(style);
                },
                true,
                {420.0f, 220.0f});
            result != 0) {
            return result;
        }
    }
    {
        ui::State<int> selected{1};
        ui::TabsStyle style;
        style.disabled.text = ui::Color{0.32f, 0.36f, 0.42f, 1.0f};
        if (const int result = verify_disabled_transition(
                "Tabs paint",
                [&] {
                    return ui::Tabs<int>{selected}
                        .tab(1, "One", ui::Spacer{280.0f, 80.0f})
                        .tab(2, "Two", ui::Spacer{280.0f, 80.0f})
                        .style(style);
                },
                false,
                {420.0f, 220.0f});
            result != 0) {
            return result;
        }
    }
    return 0;
}

int self_test() {
    if (const int result = text_contracts(); result != 0) return result;
    if (const int result = scrollbar_contracts(); result != 0) return result;
    if (const int result = popup_contracts(); result != 0) return result;
    return tabs_contracts();
}

ui::UI make_demo() {
    return ui::UI{ui::Column{
        ui::Header{"T038 — Availability Invalidation Closeout"},
        ui::Label{
            "Disabled/read-only style geometry is classified from resolved intrinsic measurement."
        }.size(12.0f),
    }.gap(12.0f)};
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    auto tree = make_demo();
    return example::run_window(
        tree, "NativeUI T038 Availability Invalidation Closeout", {700.0f, 240.0f});
}
