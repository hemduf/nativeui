#include <nativeui/nativeui.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <string>

namespace {

constexpr int kStepCount = 16;
constexpr float kSequencerPadding = 14.0f;
constexpr float kStepGap = 5.0f;
constexpr float kStepY = 48.0f;
constexpr float kStepHeight = 62.0f;

struct SequencerModel {
    std::array<bool, kStepCount> steps{
        true, false, false, false,
        true, false, true, false,
        true, false, false, false,
        true, false, true, false,
    };
    int cursor{};
    bool dragging{};
    bool drag_value{};
    int last_drag_step{-1};
};

float step_width(ui::Size size) {
    return std::max(
        1.0f,
        (size.w - kSequencerPadding * 2.0f - kStepGap * (kStepCount - 1)) /
            static_cast<float>(kStepCount));
}

ui::Rect step_rect(ui::Size size, int index) {
    const float width = step_width(size);
    return ui::Rect{
        kSequencerPadding + static_cast<float>(index) * (width + kStepGap),
        kStepY,
        width,
        kStepHeight,
    };
}

int step_at(ui::Size size, ui::Point point) {
    if (point.y < kStepY || point.y > kStepY + kStepHeight) return -1;

    const float width = step_width(size);
    const float relative = point.x - kSequencerPadding;
    if (relative < 0.0f) return -1;

    const int index = static_cast<int>(std::floor(relative / (width + kStepGap)));
    if (index < 0 || index >= kStepCount) return -1;

    const auto rect = step_rect(size, index);
    return rect.contains(point) ? index : -1;
}

} // namespace

int main() {
    ui::State<float> drive{0.35f};
    ui::State<float> tone{0.52f};
    ui::State<float> mix{0.78f};
    ui::State<bool> bypass{false};
    ui::State<std::string> preset_name{"Init"};
    SequencerModel sequencer;

    ui::UI app_ui{
        ui::Column{
            ui::Header{"NATIVEUI / PUGL + SKIA"},
            ui::Row{
                ui::Knob{"Drive", drive},
                ui::Knob{"Tone", tone},
                ui::Knob{"Mix", mix},
            }.gap(18.0f),
            ui::Toggle{"Bypass", bypass},
            ui::TextInput{"Preset name", preset_name}
                .placeholder("Type a preset name")
                .max_length(64)
                .on_submit([](const std::string& value) {
                    std::cout << "submitted: " << value << '\n';
                }),
            ui::Canvas{
                ui::Size{640.0f, 150.0f},
                [&](ui::CanvasContext2D& g) {
                    const auto size = g.size();
                    g.fill_rounded_rect(
                        ui::Rect{0.0f, 0.0f, size.w, size.h},
                        14.0f,
                        ui::colors::panel);
                    g.stroke_rounded_rect(
                        ui::Rect{0.0f, 0.0f, size.w, size.h},
                        14.0f,
                        g.focused() ? 2.0f : 1.0f,
                        g.focused() ? ui::colors::borderFocus : ui::colors::border);

                    g.text(
                        ui::Point{kSequencerPadding, 22.0f},
                        "STEP SEQUENCER",
                        13.0f,
                        ui::colors::text,
                        ui::TextAlign::Left);
                    g.text(
                        ui::Point{size.w - kSequencerPadding, 22.0f},
                        "click/drag  |  arrows  |  space  |  delete",
                        10.0f,
                        ui::colors::textMuted,
                        ui::TextAlign::Right);

                    for (int i = 0; i < kStepCount; ++i) {
                        const auto rect = step_rect(size, i);
                        const bool active = sequencer.steps[static_cast<std::size_t>(i)];
                        const bool selected = sequencer.cursor == i;

                        g.fill_rounded_rect(
                            rect,
                            7.0f,
                            active ? ui::colors::accent : ui::colors::knobInner);

                        const auto outline = selected && g.focused()
                            ? ui::colors::text
                            : ui::colors::border;
                        g.stroke_rounded_rect(
                            rect,
                            7.0f,
                            selected && g.focused() ? 2.0f : 1.0f,
                            outline);

                        g.text(
                            ui::Point{rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f},
                            std::to_string(i + 1),
                            10.0f,
                            active ? ui::colors::background : ui::colors::textMuted,
                            ui::TextAlign::Center);
                    }

                    g.text(
                        ui::Point{kSequencerPadding, size.h - 17.0f},
                        "Mouse: click or drag to add/remove   Keyboard: left/right, space, up=add, down/delete=remove",
                        10.0f,
                        ui::colors::textMuted,
                        ui::TextAlign::Left);
                }}
                .on_input([&](const ui::InputEvent& event, ui::CanvasInputContext& ctx) {
                    const auto size = ctx.size();

                    if (event.type == ui::InputType::PointerDown) {
                        const int index = step_at(size, event.position);
                        if (index < 0) return;

                        sequencer.cursor = index;
                        sequencer.drag_value = !sequencer.steps[static_cast<std::size_t>(index)];
                        sequencer.steps[static_cast<std::size_t>(index)] = sequencer.drag_value;
                        sequencer.dragging = true;
                        sequencer.last_drag_step = index;
                        ctx.capture_pointer();
                        ctx.invalidate();
                        return;
                    }

                    if (event.type == ui::InputType::PointerMove && sequencer.dragging) {
                        const int index = step_at(size, event.position);
                        if (index >= 0 && index != sequencer.last_drag_step) {
                            sequencer.cursor = index;
                            sequencer.steps[static_cast<std::size_t>(index)] = sequencer.drag_value;
                            sequencer.last_drag_step = index;
                            ctx.invalidate();
                        }
                        return;
                    }

                    if (event.type == ui::InputType::PointerUp && sequencer.dragging) {
                        sequencer.dragging = false;
                        sequencer.last_drag_step = -1;
                        ctx.release_pointer();
                        ctx.invalidate();
                        return;
                    }

                    if (event.type == ui::InputType::PointerCancel && sequencer.dragging) {
                        sequencer.dragging = false;
                        sequencer.last_drag_step = -1;
                        ctx.invalidate();
                        return;
                    }

                    if (event.type != ui::InputType::KeyDown) return;

                    switch (event.key) {
                    case ui::Key::Left:
                        sequencer.cursor = (sequencer.cursor + kStepCount - 1) % kStepCount;
                        break;
                    case ui::Key::Right:
                        sequencer.cursor = (sequencer.cursor + 1) % kStepCount;
                        break;
                    case ui::Key::Home:
                        sequencer.cursor = 0;
                        break;
                    case ui::Key::End:
                        sequencer.cursor = kStepCount - 1;
                        break;
                    case ui::Key::Space:
                    case ui::Key::Enter: {
                        auto& step = sequencer.steps[static_cast<std::size_t>(sequencer.cursor)];
                        step = !step;
                        break;
                    }
                    case ui::Key::Up:
                        sequencer.steps[static_cast<std::size_t>(sequencer.cursor)] = true;
                        break;
                    case ui::Key::Down:
                    case ui::Key::Backspace:
                    case ui::Key::Delete:
                        sequencer.steps[static_cast<std::size_t>(sequencer.cursor)] = false;
                        break;
                    case ui::Key::A:
                        if (!event.primary_shortcut()) {
                            sequencer.steps[static_cast<std::size_t>(sequencer.cursor)] = true;
                        }
                        break;
                    default:
                        return;
                    }

                    ctx.invalidate();
                })
        }.padding(24.0f).gap(18.0f)
    };

    ui::Application application;
    ui::StandaloneWindow window{
        application,
        app_ui,
        ui::WindowDesc{
            .title = "NativeUI Pugl + Skia",
            .size = {720.0f, 760.0f},
            .resizable = true,
        }
    };

    return application.run();
}
