#include "example_support.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals;

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
    ui::Tooltip normalized{"Normalized delay", ui::Button{"Action", [] {}}};
    normalized.delay(-10ms);
    if (normalized.delay() != 0ms) {
        return example::fail("negative tooltip delay was not normalized to zero");
    }

    ui::UI first{SlotRoot{{ui::make_spec(std::move(normalized))}}};
    ui::HeadlessRenderer first_renderer{{420.0f, 220.0f}};
    if (!first_renderer.render(first) || first_renderer.rgba_pixels().empty()) {
        return example::fail("public Tooltip did not render through HeadlessRenderer");
    }

    ui::UI second{SlotRoot{{ui::make_spec(
        ui::Tooltip{"Independent tooltip", Probe{false, std::make_shared<ProbeState>()}})}}};
    ui::HeadlessRenderer second_renderer{{420.0f, 220.0f}};
    if (!second_renderer.render(second) || second_renderer.rgba_pixels().empty()) {
        return example::fail("second independent Tooltip UI did not render");
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

        for (int i = 0; i < 4; ++i) (void)application.poll(0.0);
        if (!standalone.last_error().empty()) return example::fail(standalone.last_error());

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
