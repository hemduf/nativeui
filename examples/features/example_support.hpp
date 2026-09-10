#pragma once

#include <nativeui/nativeui.hpp>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace example {

class Platform final : public ui::PlatformServices {
public:
    void set_clipboard_text(std::string_view text) override { clipboard.assign(text); }
    void request_clipboard_text() override {}
    bool accept_drop(std::string_view type, ui::Rect region) override {
        accepted_drop_type.assign(type);
        accepted_drop_region = region;
        ++drop_accept_count;
        return true;
    }
    void reject_drop(ui::Rect region) override {
        rejected_drop_region = region;
        ++drop_reject_count;
    }
    std::string clipboard;
    std::string accepted_drop_type;
    ui::Rect accepted_drop_region{};
    ui::Rect rejected_drop_region{};
    int drop_accept_count{};
    int drop_reject_count{};
};

inline bool self_test_requested(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::string_view{argv[i]} == "--self-test") return true;
    }
    return false;
}

inline int fail(std::string_view message) {
    std::cerr << "example self-test failed: " << message << '\n';
    return 1;
}

inline bool near(float a, float b, float epsilon = 0.01f) {
    return std::abs(a - b) <= epsilon;
}

struct BoxObservation {
    ui::Rect bounds{};
    int paints{};
};

class BoxComponent final : public ui::Component {
public:
    BoxComponent(std::string label,
                 ui::Size minimum,
                 ui::Size preferred,
                 ui::Color color,
                 std::shared_ptr<BoxObservation> observation)
        : label_(std::move(label)),
          minimum_(minimum),
          preferred_(preferred),
          color_(color),
          observation_(std::move(observation)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return preferred_;
    }

    [[nodiscard]] ui::Size minimum_size(const std::vector<ui::ChildMetrics>&) const override {
        return minimum_;
    }

    void paint(ui::PaintContext& context) const override {
        if (observation_) {
            observation_->bounds = context.bounds();
            ++observation_->paints;
        }
        auto& p = context.painter();
        const auto b = context.bounds();
        p.fill_rounded_rect(b, 8.0f, color_);
        p.stroke_rounded_rect(b, 8.0f, 1.0f, ui::colors::border);
        p.text({b.x + b.w * 0.5f, b.y + b.h * 0.5f},
               label_,
               12.0f,
               ui::colors::text,
               ui::TextAlign::Center);
    }

private:
    std::string label_;
    ui::Size minimum_{};
    ui::Size preferred_{};
    ui::Color color_{};
    std::shared_ptr<BoxObservation> observation_;
};

class Box {
public:
    Box(std::string label,
        ui::Size preferred,
        ui::Color color = ui::colors::accent,
        ui::Size minimum = {},
        std::shared_ptr<BoxObservation> observation = {})
        : label_(std::move(label)),
          minimum_(minimum),
          preferred_(preferred),
          color_(color),
          observation_(std::move(observation)) {}

    ui::Spec spec() && {
        auto label = std::move(label_);
        const auto minimum = minimum_;
        const auto preferred = preferred_;
        const auto color = color_;
        auto observation = std::move(observation_);
        return ui::Spec{
            [label = std::move(label), minimum, preferred, color, observation]() mutable {
                return std::make_unique<BoxComponent>(
                    std::move(label), minimum, preferred, color, std::move(observation));
            },
            {}};
    }

private:
    std::string label_;
    ui::Size minimum_{};
    ui::Size preferred_{};
    ui::Color color_{};
    std::shared_ptr<BoxObservation> observation_;
};

inline ui::InputEvent key(ui::Key key, bool shift = false) {
    ui::InputEvent event{};
    event.type = ui::InputType::KeyDown;
    event.key = key;
    event.shift = shift;
    return event;
}

inline ui::InputEvent pointer(ui::InputType type, float x, float y) {
    ui::InputEvent event{};
    event.type = type;
    event.position = {x, y};
    return event;
}

inline int run_window(ui::UI& ui, std::string title, ui::Size size) {
#ifdef NATIVEUI_EXAMPLE_SELF_TEST_ONLY
    (void)ui;
    (void)title;
    (void)size;
    return fail("window mode is disabled in self-test-only validation builds");
#else
    ui::Application application;
    ui::StandaloneWindow window{
        application,
        ui,
        ui::WindowDesc{.title = std::move(title), .size = size, .resizable = true}};
    return application.run();
#endif
}

} // namespace example
