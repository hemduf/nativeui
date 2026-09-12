#include "example_support.hpp"

#include <nativeui/detail/dispatcher_owner.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// Shared demo pieces
// ---------------------------------------------------------------------------

struct DemoState {
    int clicks{};
    ui::State<std::string> status{"No interaction yet"};
};

class StatusComponent final : public ui::Component {
public:
    explicit StatusComponent(ui::State<std::string>& status) : status_(&status) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {320.0f, 20.0f};
    }

    void mount(ui::MountContext& context) override {
        subscription_ = status_->observe(
            [invalidate = context.invalidator()](const std::string&) { invalidate(); });
    }

    void unmount(ui::LifecycleContext&) override { subscription_.reset(); }

    void paint(ui::PaintContext& context) const override {
        context.painter().text(
            {context.bounds().x, context.bounds().y + context.bounds().h * 0.5f},
            status_->get(),
            13.0f,
            ui::colors::textMuted);
    }

private:
    ui::State<std::string>* status_{};
    ui::State<std::string>::Subscription subscription_;
};

class Status {
public:
    explicit Status(ui::State<std::string>& status) : status_(&status) {}

    ui::Spec spec() && {
        auto* status = status_;
        return ui::Spec{[status] { return std::make_unique<StatusComponent>(*status); }, {}};
    }

private:
    ui::State<std::string>* status_{};
};

ui::UI make_demo_ui(DemoState& state) {
    return ui::UI{
        ui::Column{
            ui::Header{"T062 — Tooltip"},
            ui::Label{
                "Hover or focus a control. The tooltip overlay never blocks the control "
                "underneath it, and both triggers share the same 500 ms delay."}
                .size(12.0f)
                .color(ui::colors::textMuted),
            ui::Tooltip{
                "Increments the click counter after the shared delay",
                ui::Button{"Click me", [&state] {
                    ++state.clicks;
                    state.status.set("Clicks: " + std::to_string(state.clicks));
                }}},
            ui::Tooltip{
                "Keyboard focus uses the same delay as hover",
                ui::Button{"Focus me", [&state] {
                    state.status.set("Activated through keyboard focus");
                }}},
            ui::Tooltip{
                "A second tooltip never reuses the first one's elapsed delay",
                ui::Button{"Third control", [&state] {
                    state.status.set("Third control activated");
                }}},
            Status{state.status},
        }.gap(12.0f).padding(16.0f)};
}

// ---------------------------------------------------------------------------
// Deterministic self-test
// ---------------------------------------------------------------------------

class SelfTestPlatform final : public ui::PlatformServices, public ui::DispatcherProvider {
public:
    SelfTestPlatform()
        : clock_(std::make_shared<ui::detail::ManualDispatcherClock>()),
          owner_({}, clock_) {}

    [[nodiscard]] ui::Dispatcher dispatcher() const noexcept override {
        return owner_.dispatcher();
    }

    [[nodiscard]] std::shared_ptr<ui::detail::ManualDispatcherClock> clock() const noexcept {
        return clock_;
    }

    void set_clipboard_text(std::string_view text) override { clipboard.assign(text); }
    void request_clipboard_text() override {}

private:
    std::shared_ptr<ui::detail::ManualDispatcherClock> clock_;

public:
    ui::detail::DispatcherOwner owner_;
    std::string clipboard;
};

struct ProbeState {
    ui::NodeId id{ui::kInvalidNodeId};
    int clicks{};
    bool focused{};
};

class ProbeComponent final : public ui::Component {
public:
    ProbeComponent(bool focusable, std::shared_ptr<ProbeState> state)
        : focusable_(focusable), state_(std::move(state)) {}

    [[nodiscard]] bool focusable() const noexcept override { return focusable_; }
    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {120.0f, 32.0f};
    }

    void mount(ui::MountContext& context) override { state_->id = context.node_id(); }
    void focus_changed(bool focused, ui::FocusContext&) override { state_->focused = focused; }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type == ui::InputType::PointerDown) {
            ++state_->clicks;
            return ui::EventResult::Handled;
        }
        return ui::EventResult::Ignored;
    }

    void paint(ui::PaintContext&) const override {}

private:
    bool focusable_{};
    std::shared_ptr<ProbeState> state_;
};

class Probe {
public:
    Probe(bool focusable, std::shared_ptr<ProbeState> state)
        : focusable_(focusable), state_(std::move(state)) {}

    ui::Spec spec() && {
        const bool focusable = focusable_;
        auto state = std::move(state_);
        return ui::Spec{
            [focusable, state = std::move(state)] {
                return std::make_unique<ProbeComponent>(focusable, state);
            },
            {}};
    }

private:
    bool focusable_{};
    std::shared_ptr<ProbeState> state_;
};

class SlotRootComponent final : public ui::Component {
public:
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {420.0f, 220.0f};
    }

    void layout_children(
        ui::Rect,
        const std::vector<ui::ChildMetrics>&,
        std::vector<ui::ChildPlacement>& placements) const override {
        if (!placements.empty()) placements[0].bounds = {20.0f, 20.0f, 120.0f, 32.0f};
        if (placements.size() > 1) placements[1].bounds = {20.0f, 60.0f, 300.0f, 140.0f};
    }

    void paint(ui::PaintContext&) const override {}
};

class SlotRoot {
public:
    explicit SlotRoot(std::vector<ui::Spec> children) : children_(std::move(children)) {}

    ui::Spec spec() && {
        return ui::Spec{[] { return std::make_unique<SlotRootComponent>(); },
                        std::move(children_)};
    }

private:
    std::vector<ui::Spec> children_;
};

int self_test() {
    SelfTestPlatform platform;

    auto hover_anchor = std::make_shared<ProbeState>();
    auto control = std::make_shared<ProbeState>();

    ui::UI ui{SlotRoot{
        {ui::make_spec(ui::Tooltip{"Hover help", Probe{false, hover_anchor}}),
         ui::make_spec(Probe{false, control})}}};
    ui.resize({420.0f, 220.0f});
    ui.activate(platform);

    // Hover trigger: exact 499/500 ms boundary through the fake T065 clock.
    (void)ui.dispatch(example::pointer(ui::InputType::PointerMove, 80.0f, 36.0f), platform);
    platform.clock()->advance(499ms);
    if (platform.owner_.checkpoint() != 0 || !ui.overlay_entries().empty()) {
        return example::fail("tooltip appeared before the 500 ms hover delay");
    }
    platform.clock()->advance(1ms);
    if (platform.owner_.checkpoint() != 1 || ui.overlay_entries().size() != 1) {
        return example::fail("tooltip did not appear at the 500 ms hover delay");
    }

    // Semantic help is available independently from the rendered overlay.
    const auto anchor = ui.overlay_entries().front().anchor;
    if (!anchor.has_value()) return example::fail("tooltip overlay has no anchor");
    const auto semantics = ui.component_semantics(*anchor);
    if (!semantics.has_value() || semantics->description != "Hover help") {
        return example::fail("tooltip help text is not exposed as anchor semantics");
    }

    // The overlay must not consume the pointer press of the control below it.
    ui.resize({420.0f, 220.0f});
    const auto bounds = ui.overlay_entries().front().bounds;
    const ui::Point through{bounds.x + 10.0f, 70.0f};
    if (!bounds.contains(through)) return example::fail("tooltip overlay missed the click probe");
    (void)ui.dispatch(example::pointer(ui::InputType::PointerDown, through.x, through.y),
                      platform);
    if (control->clicks != 1) {
        return example::fail("non-hit-test tooltip blocked the control underneath");
    }
    if (!ui.overlay_entries().empty()) {
        return example::fail("PointerDown did not dismiss the visible tooltip");
    }

    // Focus trigger uses the same configured delay.
    SelfTestPlatform focus_platform;
    auto focus_anchor = std::make_shared<ProbeState>();
    ui::UI focused_ui{SlotRoot{
        {ui::make_spec(ui::Tooltip{"Focus help", Probe{true, focus_anchor}})}}};
    focused_ui.resize({420.0f, 220.0f});
    focused_ui.activate(focus_platform);
    if (!focus_anchor->focused) return example::fail("focus did not reach the focus anchor");
    focus_platform.clock()->advance(499ms);
    if (focus_platform.owner_.checkpoint() != 0 || !focused_ui.overlay_entries().empty()) {
        return example::fail("tooltip appeared before the 500 ms focus delay");
    }
    focus_platform.clock()->advance(1ms);
    if (focus_platform.owner_.checkpoint() != 1 || focused_ui.overlay_entries().size() != 1) {
        return example::fail("tooltip did not appear at the 500 ms focus delay");
    }

    // An empty help string never shows a visual tooltip.
    SelfTestPlatform empty_platform;
    ui::UI empty{SlotRoot{
        {ui::make_spec(ui::Tooltip{"", Probe{false, std::make_shared<ProbeState>()}})}}};
    empty.resize({420.0f, 220.0f});
    empty.activate(empty_platform);
    (void)empty.dispatch(example::pointer(ui::InputType::PointerMove, 80.0f, 36.0f),
                         empty_platform);
    empty_platform.clock()->advance(600ms);
    if (empty_platform.owner_.checkpoint() != 0 || !empty.overlay_entries().empty()) {
        return example::fail("empty tooltip text still produced a visual overlay");
    }

    // Two independent UIs never share tooltip timing or state.
    SelfTestPlatform first_platform;
    SelfTestPlatform second_platform;
    ui::UI first{SlotRoot{
        {ui::make_spec(ui::Tooltip{"First help", Probe{false, std::make_shared<ProbeState>()}})}}};
    ui::UI second{SlotRoot{
        {ui::make_spec(ui::Tooltip{"Second help", Probe{false, std::make_shared<ProbeState>()}})}}};
    first.resize({420.0f, 220.0f});
    second.resize({420.0f, 220.0f});
    first.activate(first_platform);
    second.activate(second_platform);
    (void)first.dispatch(example::pointer(ui::InputType::PointerMove, 80.0f, 36.0f),
                         first_platform);
    (void)second.dispatch(example::pointer(ui::InputType::PointerMove, 80.0f, 36.0f),
                          second_platform);
    first_platform.clock()->advance(300ms);
    second_platform.clock()->advance(500ms);
    second_platform.owner_.checkpoint();
    first_platform.owner_.checkpoint();
    if (!first.overlay_entries().empty()) {
        return example::fail("first UI tooltip appeared without its own delay elapsing");
    }
    if (second.overlay_entries().size() != 1) {
        return example::fail("second UI tooltip did not appear independently");
    }

    return 0;
}

int platform_smoke() {
    const char* stage = "application";
    try {
        ui::Application application;
        if (!application.valid()) {
            return example::fail(application.last_error().empty()
                                     ? "T062 platform application is invalid"
                                     : application.last_error());
        }

        stage = "standalone";
        auto standalone_anchor = std::make_shared<ProbeState>();
        ui::UI standalone_ui{SlotRoot{
            {ui::make_spec(ui::Tooltip{"Standalone help", Probe{true, standalone_anchor}})}}};
        ui::StandaloneWindow standalone{
            application,
            standalone_ui,
            ui::WindowDesc{
                .title = "NativeUI T062 platform smoke",
                .size = {420.0f, 220.0f},
                .resizable = true}};
        if (!standalone.valid() || !standalone.native_handle()) {
            return example::fail(standalone.last_error().empty()
                                     ? "T062 standalone window is invalid"
                                     : standalone.last_error());
        }

        // Arm the tooltip through the real standalone PlatformServices object,
        // then let the native event loop wait for the T065 timer to fire. A
        // native focus-out may deactivate the view mid-wait (the window is not
        // guaranteed to be frontmost in every environment), so re-activate and
        // re-arm deterministically instead of assuming one activation sticks.
        bool standalone_shown = false;
        for (int attempt = 0; attempt < 4 && !standalone_shown; ++attempt) {
            standalone_ui.activate(standalone);
            (void)standalone_ui.dispatch(
                example::pointer(ui::InputType::PointerMove, 10.0f, 10.0f), standalone);
            (void)standalone_ui.dispatch(
                example::pointer(ui::InputType::PointerMove, 80.0f, 36.0f), standalone);
            for (int i = 0; i < 12; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds{60});
                (void)application.poll(0.0);
                if (!standalone_ui.overlay_entries().empty()) {
                    standalone_shown = true;
                    break;
                }
            }
        }
        if (!standalone_shown || standalone_ui.overlay_entries().size() != 1) {
            return example::fail(standalone.last_error().empty()
                                     ? "standalone tooltip did not appear through the native dispatcher"
                                     : standalone.last_error());
        }
        if (standalone_ui.overlay_entries().front().pointer_policy !=
            ui::OverlayPointerPolicy::Ignore) {
            return example::fail("standalone tooltip is not non-hit-test");
        }

        // Real native paint cycles must keep the surface and window valid.
        for (int i = 0; i < 4; ++i) (void)application.poll(0.0);
        if (!standalone.last_error().empty()) return example::fail(standalone.last_error());

        // PointerDown on the anchor dismisses through the native platform path.
        (void)standalone_ui.dispatch(
            example::pointer(ui::InputType::PointerDown, 80.0f, 36.0f), standalone);
        if (!standalone_ui.overlay_entries().empty()) {
            return example::fail("PointerDown did not dismiss the native tooltip");
        }

        stage = "embedded";
        auto embedded_anchor = std::make_shared<ProbeState>();
        ui::UI embedded_ui{SlotRoot{
            {ui::make_spec(ui::Tooltip{"Embedded help", Probe{true, embedded_anchor}})}}};
        ui::EmbeddedView embedded{
            embedded_ui, standalone.native_handle(), {420.0f, 220.0f}};
        if (!embedded.native_handle()) {
            return example::fail(embedded.last_error().empty()
                                     ? "T062 embedded view is invalid"
                                     : embedded.last_error());
        }

        bool embedded_shown = false;
        for (int attempt = 0; attempt < 4 && !embedded_shown; ++attempt) {
            embedded_ui.activate(embedded);
            (void)embedded_ui.dispatch(
                example::pointer(ui::InputType::PointerMove, 10.0f, 10.0f), embedded);
            (void)embedded_ui.dispatch(
                example::pointer(ui::InputType::PointerMove, 80.0f, 36.0f), embedded);
            for (int i = 0; i < 20; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds{40});
                (void)embedded.poll();
                if (!embedded_ui.overlay_entries().empty()) {
                    embedded_shown = true;
                    break;
                }
            }
        }
        if (!embedded_shown || embedded_ui.overlay_entries().size() != 1) {
            return example::fail(embedded.last_error().empty()
                                     ? "embedded tooltip did not appear through the native dispatcher"
                                     : embedded.last_error());
        }
        for (int i = 0; i < 4; ++i) (void)embedded.poll();
        if (!embedded.last_error().empty()) return example::fail(embedded.last_error());
        return 0;
    } catch (const std::exception& error) {
        return example::fail(std::string{"T062 platform smoke "} + stage + ": " + error.what());
    } catch (...) {
        return example::fail(std::string{"T062 platform smoke "} + stage + ": unknown exception");
    }
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    if (argc == 2 && std::string_view{argv[1]} == "--platform-smoke") return platform_smoke();

    DemoState state;
    ui::UI ui = make_demo_ui(state);
    return example::run_window(
        ui, "T062 — Tooltip", ui::Size{460.0f, 320.0f});
}
