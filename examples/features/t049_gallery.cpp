#include "example_support.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::array<std::byte, 75> kDemoPng{
    std::byte{137}, std::byte{80}, std::byte{78}, std::byte{71}, std::byte{13}, std::byte{10},
    std::byte{26}, std::byte{10}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{13},
    std::byte{73}, std::byte{72}, std::byte{68}, std::byte{82}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{2},
    std::byte{8}, std::byte{6}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{114},
    std::byte{182}, std::byte{13}, std::byte{36}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{18}, std::byte{73}, std::byte{68}, std::byte{65}, std::byte{84}, std::byte{120},
    std::byte{218}, std::byte{99}, std::byte{248}, std::byte{207}, std::byte{192}, std::byte{240},
    std::byte{31}, std::byte{12}, std::byte{129}, std::byte{52}, std::byte{24}, std::byte{0},
    std::byte{0}, std::byte{73}, std::byte{200}, std::byte{9}, std::byte{247}, std::byte{3},
    std::byte{217}, std::byte{100}, std::byte{241}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{73}, std::byte{69}, std::byte{78}, std::byte{68}, std::byte{174},
    std::byte{66}, std::byte{96}, std::byte{130},
};

constexpr std::string_view kDemoSvg = R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 32 32">
  <circle cx="16" cy="16" r="14" fill="#326aa0"/>
  <path d="M9 17 L14 22 L24 10" fill="none" stroke="#ffffff" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"/>
</svg>)svg";

std::vector<std::byte> svg_bytes() {
    const auto* begin = reinterpret_cast<const std::byte*>(kDemoSvg.data());
    return {begin, begin + kDemoSvg.size()};
}

ui::SvgIcon make_demo_icon() {
    const auto bytes = svg_bytes();
    return ui::SvgIcon::parse(std::span<const std::byte>{bytes.data(), bytes.size()});
}

ui::ButtonStyle showcase_button_style() {
    ui::ButtonStyle style;
    style.base.fill = ui::Color{0.10f, 0.14f, 0.20f, 1.0f};
    style.hovered.fill = ui::Color{0.16f, 0.28f, 0.42f, 1.0f};
    style.pressed.fill = ui::Color{0.20f, 0.48f, 0.72f, 1.0f};
    style.focused.border = ui::Color{0.64f, 0.84f, 1.0f, 1.0f};
    style.disabled.fill = ui::Color{0.12f, 0.13f, 0.15f, 1.0f};
    style.disabled.text = ui::Color{0.42f, 0.44f, 0.48f, 1.0f};
    return style;
}

struct GalleryState {
    ui::State<int> section{1};
    ui::State<std::string> text{"NativeUI"};
    ui::State<std::string> notes{"A deterministic component gallery."};
    ui::State<bool> toggle{true};
    ui::State<bool> checked{true};
    ui::State<bool> disabled_enabled{false};
    ui::State<int> radio_selection{2};
    ui::RadioGroup<int> radio_group{radio_selection};
    ui::State<int> state_radio_selection{2};
    ui::RadioGroup<int> state_radio_group{state_radio_selection};
    ui::State<float> knob{0.58f};
    ui::State<float> slider{0.42f};
    ui::State<ui::RangeValue> range{ui::RangeValue{0.24f, 0.76f}};
    ui::State<float> progress{0.64f};
    ui::State<int> combo{2};
    ui::State<std::optional<int>> list_selection{2};
    ui::State<int> nested_tab{1};
    ui::ScrollState scroll{ui::ScrollAxis::Horizontal};
    ui::Image image{ui::Image::decode(kDemoPng)};
    ui::SvgIcon icon{make_demo_icon()};
    int button_activations{};
    int menu_actions{};
};

ui::Canvas card(std::string label, ui::Color color, float width = 170.0f) {
    return ui::Canvas{
        width,
        66.0f,
        [label = std::move(label), color](ui::CanvasContext2D& g) {
            g.fill_rounded_rect({0.0f, 0.0f, g.width(), g.height()}, 8.0f, color);
            g.text({g.width() * 0.5f, g.height() * 0.5f}, label, 12.0f,
                   ui::colors::text, ui::TextAlign::Center);
        }};
}

ui::UI make_gallery(GalleryState& state) {
    const auto button_style = showcase_button_style();
    const auto image = state.image;
    const auto icon = state.icon;

    return ui::UI{
        ui::Column{
            ui::Header{"T049 — NativeUI Component Gallery"},
            ui::Label{
                "One public-API-only window for visual discovery and manual QA. Focused tNNN examples remain the canonical API demonstrations."
            }.size(12.0f).color(ui::colors::textMuted),
            ui::Tabs<int>{state.section}
                .tab(
                    1,
                    "Layout",
                    ui::Column{
                        ui::Label{"LAYOUT / CONTAINERS"}.size(11.0f).bold().color(ui::colors::textMuted),
                        ui::Row{
                            ui::Padding{8.0f, card("Padding", ui::colors::panel, 150.0f)},
                            ui::Flex{card("Flex grow", ui::colors::input, 130.0f)}.grow(1.0f)
                        }.gap(10.0f),
                        ui::Grid{
                            ui::GridTracks{
                                {ui::Track::fixed(150.0f), ui::Track::flex(1.0f)},
                                {ui::Track::auto_size(), ui::Track::auto_size()}},
                            card("Grid fixed", ui::colors::panel, 130.0f),
                            card("Grid flex", ui::colors::input, 130.0f),
                            card("Grid row 2", ui::colors::input, 130.0f),
                            card("Grid row 2", ui::colors::panel, 130.0f)}
                            .gap(8.0f),
                        ui::Clip{
                            ui::Stack{
                                card("Stack background", ui::Color{0.08f, 0.12f, 0.18f, 1.0f}, 360.0f),
                                ui::Canvas{360.0f, 66.0f, [](ui::CanvasContext2D& g) {
                                    g.line({18.0f, 50.0f}, {342.0f, 16.0f}, 3.0f, ui::colors::accent);
                                    g.text({180.0f, 33.0f}, "Stack + Clip", 12.0f,
                                           ui::colors::text, ui::TextAlign::Center);
                                }}
                            }}
                    }.gap(12.0f).padding(10.0f))
                .tab(
                    2,
                    "Controls",
                    ui::Column{
                        ui::Label{"TEXT / CONTROLS / VALUES"}.size(11.0f).bold().color(ui::colors::textMuted),
                        ui::TextInput{"Name", state.text}.placeholder("Type here"),
                        ui::TextArea{"Notes", state.notes}.placeholder("Multiline text"),
                        ui::Row{
                            ui::Button{"Action", [&state] { ++state.button_activations; }}.style(button_style),
                            ui::Toggle{"Toggle", state.toggle},
                            ui::Checkbox{state.checked, "Checked"},
                            ui::RadioButton{state.radio_group, 1, "A"},
                            ui::RadioButton{state.radio_group, 2, "B"}
                        }.gap(9.0f),
                        ui::Row{
                            ui::Knob{"Knob", state.knob},
                            ui::Slider{state.slider}.range(0.0f, 1.0f),
                            ui::RangeSlider{state.range}.range(0.0f, 1.0f)
                        }.gap(14.0f),
                        ui::Row{
                            ui::ProgressBar{state.progress},
                            ui::Meter{state.progress},
                            ui::Meter{state.progress}.orientation(ui::ProgressOrientation::Vertical)
                        }.gap(14.0f)
                    }.gap(12.0f).padding(10.0f))
                .tab(
                    3,
                    "Collections",
                    ui::Column{
                        ui::Label{"COLLECTIONS / NAVIGATION"}.size(11.0f).bold().color(ui::colors::textMuted),
                        ui::Row{
                            ui::ComboBox<int>{
                                state.combo,
                                {{1, "One", true}, {2, "Two", true}, {3, "Disabled", false}}},
                            ui::PopupMenu{
                                "Actions",
                                {
                                    ui::PopupMenuItem::action("Increment", [&state] { ++state.menu_actions; }),
                                    ui::PopupMenuItem::separator(),
                                    ui::PopupMenuItem::action("Disabled", [] {}, false),
                                }}
                        }.gap(12.0f),
                        ui::ScrollView{
                            state.scroll,
                            ui::Row{
                                card("Scroll A", ui::colors::panel, 220.0f),
                                card("Scroll B", ui::colors::input, 220.0f),
                                card("Scroll C", ui::colors::panel, 220.0f)
                            }.gap(10.0f)}
                            .pointer_pan(true),
                        ui::Row{
                            ui::ListView<int>{state.list_selection}
                                .item(1, card("List one", ui::colors::panel, 210.0f))
                                .item(2, card("List selected", ui::colors::input, 210.0f))
                                .item(3, card("List disabled", ui::colors::panel, 210.0f), false),
                            ui::Tabs<int>{state.nested_tab}
                                .tab(1, "First", card("Nested tab", ui::colors::panel, 260.0f))
                                .tab(2, "Second", card("Second panel", ui::colors::input, 260.0f))
                        }.gap(18.0f)
                    }.gap(12.0f).padding(10.0f))
                .tab(
                    4,
                    "Rendering + states",
                    ui::Column{
                        ui::Label{"RENDERING / RESOURCES"}.size(11.0f).bold().color(ui::colors::textMuted),
                        ui::Canvas{620.0f, 180.0f, [image, icon](ui::CanvasContext2D& g) {
                            g.fill_rounded_rect({0.0f, 0.0f, g.width(), g.height()}, 10.0f, ui::colors::panel);
                            g.draw_image(image, {20.0f, 22.0f, 116.0f, 116.0f}, ui::ImageFit::Contain);
                            g.draw_svg(icon, {164.0f, 22.0f, 116.0f, 116.0f});
                            g.fill_rounded_rect({310.0f, 34.0f, 120.0f, 92.0f}, 14.0f, ui::colors::input);
                            g.line({448.0f, 122.0f}, {590.0f, 42.0f}, 4.0f, ui::colors::accent);
                            g.text({370.0f, 150.0f}, "Canvas / Image / SVG", 12.0f,
                                   ui::colors::textMuted, ui::TextAlign::Center);
                        }},
                        ui::Label{"T038 INTERACTION / STYLE STATES"}.size(11.0f).bold().color(ui::colors::textMuted),
                        ui::Row{
                            ui::Button{"Normal / hover / press / focus", [] {}}.style(button_style),
                            ui::Enabled{
                                state.disabled_enabled,
                                ui::Button{"Disabled", [] {}}.style(button_style)},
                            ui::Checkbox{state.checked, "Selected"},
                            ui::RadioButton{state.state_radio_group, 2, "Selected radio"}
                        }.gap(10.0f),
                        ui::Label{
                            "Use pointer and keyboard focus to inspect hover, pressed and focused variants; disabled and selected states are shown explicitly."
                        }.size(11.0f).color(ui::colors::textMuted)
                    }.gap(12.0f).padding(10.0f))
        }.gap(12.0f).padding(16.0f)};
}

int self_test() {
    GalleryState state;
    if (!state.image.valid()) return example::fail("gallery in-memory Image decode failed");
    if (!state.icon.valid()) return example::fail("gallery in-memory SVG parse failed");

    auto tree = make_gallery(state);
    example::Platform platform;
    tree.resize({980.0f, 720.0f});
    tree.activate(platform);

    ui::HeadlessRenderer renderer{{980.0f, 720.0f}, 1.0f};
    for (int section = 1; section <= 4; ++section) {
        state.section.set(section);
        if (!renderer.render(tree)) {
            return example::fail("gallery section headless render failed");
        }
    }

    state.section.set(1);
    if (tree.dispatch(example::key(ui::Key::Right), platform) != ui::EventResult::Handled ||
        state.section.get() != 2) {
        return example::fail("gallery Tabs keyboard navigation failed");
    }

    state.slider.set(0.81f);
    state.range.set(ui::RangeValue{0.20f, 0.88f});
    state.combo.set(1);
    state.list_selection.set(1);
    state.checked.set(false);
    if (!renderer.render(tree)) {
        return example::fail("gallery state mutation render failed");
    }

    GalleryState other;
    state.slider.set(0.93f);
    state.combo.set(3);
    if (other.slider.get() == state.slider.get() || other.combo.get() == state.combo.get()) {
        return example::fail("gallery instances unexpectedly shared mutable state");
    }

    auto second_tree = make_gallery(other);
    second_tree.resize({980.0f, 720.0f});
    ui::HeadlessRenderer second_renderer{{980.0f, 720.0f}, 1.0f};
    if (!second_renderer.render(second_tree)) {
        return example::fail("second gallery instance render failed");
    }

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    GalleryState state;
    if (!state.image.valid()) return example::fail("gallery in-memory Image decode failed");
    if (!state.icon.valid()) return example::fail("gallery in-memory SVG parse failed");

    auto tree = make_gallery(state);
    return example::run_window(tree, "NativeUI T049 Component Gallery", {1080.0f, 760.0f});
}
