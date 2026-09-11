#pragma once

#include <nativeui/nativeui.hpp>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace test {

class Failure final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

inline void check(bool condition, const char* expression, const char* file, int line) {
    if (condition) return;
    throw Failure(std::string(file) + ":" + std::to_string(line) +
                  ": CHECK failed: " + expression);
}

inline void check_near(float actual, float expected, float epsilon,
                       const char* actual_expression, const char* expected_expression,
                       const char* file, int line) {
    if (std::fabs(actual - expected) <= epsilon) return;
    throw Failure(std::string(file) + ":" + std::to_string(line) +
                  ": CHECK_NEAR failed: " + actual_expression + "=" +
                  std::to_string(actual) + ", " + expected_expression + "=" +
                  std::to_string(expected));
}

class MockPlatform final : public ui::PlatformServices {
public:
    float text_width(std::string_view text, float size) override {
        return static_cast<float>(text.size()) * size * 0.5f;
    }

    void set_text_input(bool active, ui::Rect area, float cursor_offset) override {
        text_input_active = active;
        text_input_area = area;
        text_input_cursor_offset = cursor_offset;
        text_input_transitions.push_back(active);
    }

    void begin_pointer_capture() noexcept override { ++pointer_capture_begin_count; }
    void end_pointer_capture() noexcept override { ++pointer_capture_end_count; }

    void set_clipboard_text(std::string_view text) override {
        clipboard.assign(text);
        ++clipboard_write_count;
    }

    void request_clipboard_text() override {
        paste_requested = true;
        ++paste_request_count;
    }

    bool accept_drop(std::string_view type, ui::Rect region) override {
        accepted_drop_type.assign(type);
        accepted_drop_region = region;
        ++drop_accept_count;
        return drop_accept_result;
    }

    void reject_drop(ui::Rect region) override {
        rejected_drop_region = region;
        ++drop_reject_count;
    }

    bool text_input_active{};
    bool paste_requested{};
    float text_input_cursor_offset{};
    ui::Rect text_input_area{};
    std::string clipboard;
    int clipboard_write_count{};
    int paste_request_count{};
    bool drop_accept_result{true};
    std::string accepted_drop_type;
    ui::Rect accepted_drop_region{};
    ui::Rect rejected_drop_region{};
    int drop_accept_count{};
    int drop_reject_count{};
    int pointer_capture_begin_count{};
    int pointer_capture_end_count{};
    std::vector<bool> text_input_transitions;
};

inline ui::InputEvent key(ui::Key k, bool shift = false, bool primary = false,
                          bool alt = false) {
    ui::InputEvent event{};
    event.type = ui::InputType::KeyDown;
    event.key = k;
    event.shift = shift;
    event.ctrl = primary;
    event.primary = primary;
    event.alt = alt;
    return event;
}

inline ui::InputEvent text(std::string value) {
    ui::InputEvent event{};
    event.type = ui::InputType::TextInput;
    event.text = std::move(value);
    return event;
}

inline ui::InputEvent pointer(ui::InputType type, float x, float y, int clicks = 1) {
    ui::InputEvent event{};
    event.type = type;
    event.position = {x, y};
    event.clicks = clicks;
    return event;
}

struct ProbeState {
    int focus_in{};
    int focus_out{};
    int key_events{};
    bool focused{};
    ui::EventResult input_result{ui::EventResult::Ignored};
    std::vector<ui::Rect> focus_bounds;
};

class ProbeComponent final : public ui::Component {
public:
    explicit ProbeComponent(std::shared_ptr<ProbeState> state) : state_(std::move(state)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {100.0f, 40.0f};
    }

    void focus_changed(bool focused, ui::FocusContext& context) override {
        state_->focused = focused;
        focused ? ++state_->focus_in : ++state_->focus_out;
        state_->focus_bounds.push_back(context.bounds());
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type == ui::InputType::KeyDown) ++state_->key_events;
        return state_->input_result;
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<ProbeState> state_;
};

class Probe {
public:
    explicit Probe(std::shared_ptr<ProbeState> state) : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] { return std::make_unique<ProbeComponent>(state); },
            {}};
    }

private:
    std::shared_ptr<ProbeState> state_;
};

inline int run(const char* suite, void (*body)()) {
    try {
        body();
        std::cout << "PASS " << suite << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << suite << ": " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

} // namespace test

#define NUI_CHECK(expr) ::test::check(static_cast<bool>(expr), #expr, __FILE__, __LINE__)
#define NUI_CHECK_NEAR(actual, expected, epsilon) \
    ::test::check_near(static_cast<float>(actual), static_cast<float>(expected), \
                       static_cast<float>(epsilon), #actual, #expected, __FILE__, __LINE__)
