#include "t036_list_tabs_contract.inc"

#include <nativeui/nativeui.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <utility>

namespace {

constexpr ui::Color kHoverSurface{0.145f, 0.155f, 0.175f, 1.0f};

bool pixel_matches(ui::Rgba8 pixel, ui::Color color, int tolerance = 3) {
    const auto channel = [](float value) {
        return static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
    };
    return std::abs(static_cast<int>(pixel.r) - channel(color.r)) <= tolerance &&
           std::abs(static_cast<int>(pixel.g) - channel(color.g)) <= tolerance &&
           std::abs(static_cast<int>(pixel.b) - channel(color.b)) <= tolerance;
}

class CountedRowComponent final : public ui::Component {
public:
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {120.0f, 20.0f};
    }

    void paint(ui::PaintContext&) const override {}
};

class CountedRow final {
public:
    explicit CountedRow(int& constructions) : constructions_(&constructions) {}

    ui::Spec spec() && {
        auto* constructions = constructions_;
        return ui::Spec{
            [constructions] {
                ++*constructions;
                return std::make_unique<CountedRowComponent>();
            },
            {}};
    }

private:
    int* constructions_{};
};

struct HoverLifetimeLog {
    int pointer_moves{};
    int pointer_leaves{};
    int destructions{};
};

class HoverLifetimeProbeComponent final : public ui::Component {
public:
    explicit HoverLifetimeProbeComponent(std::shared_ptr<HoverLifetimeLog> log)
        : log_(std::move(log)) {}

    ~HoverLifetimeProbeComponent() override { ++log_->destructions; }

    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {80.0f, 30.0f};
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type == ui::InputType::PointerMove) {
            ++log_->pointer_moves;
        } else if (event.type == ui::InputType::PointerLeave) {
            ++log_->pointer_leaves;
        }
        return ui::EventResult::Ignored;
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<HoverLifetimeLog> log_;
};

class HoverLifetimeProbe final {
public:
    explicit HoverLifetimeProbe(std::shared_ptr<HoverLifetimeLog> log)
        : log_(std::move(log)) {}

    ui::Spec spec() && {
        auto log = std::move(log_);
        return ui::Spec{
            [log = std::move(log)] { return std::make_unique<HoverLifetimeProbeComponent>(log); },
            {}};
    }

private:
    std::shared_ptr<HoverLifetimeLog> log_;
};

void fully_retained_large_list_baseline() {
    constexpr int kItemCount = 256;
    ui::State<std::optional<int>> selected{std::nullopt};
    int constructions = 0;
    auto list = ui::ListView<int>{selected};
    for (int index = 0; index < kItemCount; ++index) {
        std::move(list).item(index, CountedRow{constructions});
    }

    ui::UI tree{std::move(list)};
    test::MockPlatform platform;
    tree.resize({120.0f, 100.0f});
    tree.activate(platform);

    // T036 is deliberately O(N) and non-virtualized. T067 owns the future
    // viewport-bounded materialization policy, so every baseline row must exist.
    NUI_CHECK(constructions == kItemCount);
}

void disabled_and_two_instance_contract() {
    test::MockPlatform platform;

    ui::State<bool> list_enabled{false};
    ui::State<std::optional<int>> list_selected{1};
    ui::UI disabled_list{ui::Enabled{
        list_enabled,
        ui::ListView<int>{list_selected}
            .item(1, ui::Spacer{100.0f, 30.0f})
            .item(2, ui::Spacer{100.0f, 30.0f})}};
    disabled_list.resize({100.0f, 60.0f});
    disabled_list.activate(platform);
    NUI_CHECK(disabled_list.dispatch(test::key(ui::Key::Down), platform) ==
              ui::EventResult::Ignored);
    NUI_CHECK(list_selected.get() && *list_selected.get() == 1);

    ui::State<bool> tabs_enabled{false};
    ui::State<int> disabled_tab_selected{1};
    ui::UI disabled_tabs{ui::Enabled{
        tabs_enabled,
        ui::Tabs<int>{disabled_tab_selected}
            .tab(1, "One", ui::Spacer{100.0f, 40.0f})
            .tab(2, "Two", ui::Spacer{100.0f, 40.0f})}};
    disabled_tabs.resize({200.0f, 100.0f});
    disabled_tabs.activate(platform);
    NUI_CHECK(disabled_tabs.dispatch(test::key(ui::Key::Right), platform) ==
              ui::EventResult::Ignored);
    NUI_CHECK(disabled_tab_selected.get() == 1);

    ui::State<int> left_selected{1};
    ui::State<int> right_selected{10};
    ui::UI left{ui::Tabs<int>{left_selected}
        .tab(1, "One", ui::Spacer{100.0f, 40.0f})
        .tab(2, "Two", ui::Spacer{100.0f, 40.0f})};
    ui::UI right{ui::Tabs<int>{right_selected}
        .tab(10, "Ten", ui::Spacer{100.0f, 40.0f})
        .tab(20, "Twenty", ui::Spacer{100.0f, 40.0f})};
    left.resize({200.0f, 100.0f});
    right.resize({200.0f, 100.0f});
    left.activate(platform);
    right.activate(platform);
    NUI_CHECK(left.dispatch(test::key(ui::Key::Right), platform) == ui::EventResult::Handled);
    NUI_CHECK(left_selected.get() == 2);
    NUI_CHECK(right_selected.get() == 10);
}

void external_list_selection_reveals_selected_row() {
    ui::State<std::optional<int>> selected{1};
    ui::UI tree{ui::ListView<int>{selected}
        .item(1, ui::Spacer{120.0f, 30.0f})
        .item(2, ui::Spacer{120.0f, 30.0f})
        .item(3, ui::Spacer{120.0f, 30.0f})
        .item(4, ui::Spacer{120.0f, 30.0f})
        .item(5, ui::Spacer{120.0f, 30.0f})};
    test::MockPlatform platform;
    tree.resize({120.0f, 60.0f});
    tree.activate(platform);

    ui::HeadlessRenderer renderer{{120.0f, 60.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(pixel_matches(renderer.pixel(8, 15), ui::colors::accent));

    // T036 requires selection changes, not only user-navigation writes, to
    // keep the selected row visible through the shared T034 ScrollState path.
    selected.set(5);
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(pixel_matches(renderer.pixel(8, 45), ui::colors::accent));
}

void hover_presentation_contract() {
    test::MockPlatform platform;

    {
        ui::State<std::optional<int>> selected{std::nullopt};
        ui::UI tree{ui::ListView<int>{selected}
            .item(1, ui::Spacer{120.0f, 30.0f})
            .item(2, ui::Spacer{120.0f, 30.0f}, false)
            .item(3, ui::Spacer{120.0f, 30.0f})};
        tree.resize({120.0f, 90.0f});
        tree.activate(platform);
        ui::HeadlessRenderer renderer{{120.0f, 90.0f}, 1.0f};

        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(!pixel_matches(renderer.pixel(20, 15), kHoverSurface));
        (void)tree.dispatch(
            test::pointer(ui::InputType::PointerMove, 20.0f, 15.0f), platform);
        NUI_CHECK(!selected.get());
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(pixel_matches(renderer.pixel(20, 15), kHoverSurface));

        // Disabled rows are never visually hovered and moving onto one clears
        // the previous enabled-row hover without mutating selection.
        (void)tree.dispatch(
            test::pointer(ui::InputType::PointerMove, 20.0f, 45.0f), platform);
        NUI_CHECK(!selected.get());
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(!pixel_matches(renderer.pixel(20, 15), kHoverSurface));
        NUI_CHECK(!pixel_matches(renderer.pixel(20, 45), kHoverSurface));

        (void)tree.dispatch(
            test::pointer(ui::InputType::PointerMove, 20.0f, 75.0f), platform);
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(pixel_matches(renderer.pixel(20, 75), kHoverSurface));

        ui::InputEvent leave{};
        leave.type = ui::InputType::PointerLeave;
        leave.position = {120.0f, 75.0f};
        NUI_CHECK(tree.dispatch(leave, platform) == ui::EventResult::Handled);
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(!pixel_matches(renderer.pixel(20, 75), kHoverSurface));
    }

    {
        ui::State<int> selected{1};
        ui::UI tree{ui::Tabs<int>{selected}
            .tab(1, "One", ui::Spacer{300.0f, 50.0f})
            .tab(2, "Disabled", ui::Spacer{300.0f, 50.0f}, false)
            .tab(3, "Three", ui::Spacer{300.0f, 50.0f})};
        tree.resize({300.0f, 100.0f});
        tree.activate(platform);
        ui::HeadlessRenderer renderer{{300.0f, 100.0f}, 1.0f};

        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(pixel_matches(renderer.pixel(220, 20), ui::colors::panel));
        (void)tree.dispatch(
            test::pointer(ui::InputType::PointerMove, 220.0f, 20.0f), platform);
        NUI_CHECK(selected.get() == 1);
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(pixel_matches(renderer.pixel(220, 20), kHoverSurface));

        (void)tree.dispatch(
            test::pointer(ui::InputType::PointerMove, 150.0f, 20.0f), platform);
        NUI_CHECK(selected.get() == 1);
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(!pixel_matches(renderer.pixel(220, 20), kHoverSurface));
        NUI_CHECK(!pixel_matches(renderer.pixel(150, 20), kHoverSurface));

        (void)tree.dispatch(
            test::pointer(ui::InputType::PointerMove, 220.0f, 20.0f), platform);
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(pixel_matches(renderer.pixel(220, 20), kHoverSurface));
        ui::InputEvent leave{};
        leave.type = ui::InputType::PointerLeave;
        leave.position = {300.0f, 20.0f};
        NUI_CHECK(tree.dispatch(leave, platform) == ui::EventResult::Handled);
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(!pixel_matches(renderer.pixel(220, 20), kHoverSurface));
    }

    // Hover bookkeeping belongs to each retained Tree/component instance.
    ui::State<int> left_selected{1};
    ui::State<int> right_selected{1};
    ui::UI left{ui::Tabs<int>{left_selected}
        .tab(1, "One", ui::Spacer{200.0f, 40.0f})
        .tab(2, "Two", ui::Spacer{200.0f, 40.0f})};
    ui::UI right{ui::Tabs<int>{right_selected}
        .tab(1, "One", ui::Spacer{200.0f, 40.0f})
        .tab(2, "Two", ui::Spacer{200.0f, 40.0f})};
    left.resize({200.0f, 100.0f});
    right.resize({200.0f, 100.0f});
    left.activate(platform);
    right.activate(platform);
    (void)left.dispatch(
        test::pointer(ui::InputType::PointerMove, 150.0f, 20.0f), platform);
    ui::HeadlessRenderer left_renderer{{200.0f, 100.0f}, 1.0f};
    ui::HeadlessRenderer right_renderer{{200.0f, 100.0f}, 1.0f};
    NUI_CHECK(left_renderer.render(left));
    NUI_CHECK(right_renderer.render(right));
    // Sample away from the centered tab label so this checks the hover surface,
    // not a foreground text glyph rendered on top of it.
    NUI_CHECK(pixel_matches(left_renderer.pixel(120, 20), kHoverSurface));
    NUI_CHECK(!pixel_matches(right_renderer.pixel(120, 20), kHoverSurface));
}

void dynamic_hover_lifetime_contract() {
    auto log = std::make_shared<HoverLifetimeLog>();
    ui::State<bool> visible{true};
    test::MockPlatform platform;
    ui::UI tree{ui::If{visible, HoverLifetimeProbe{log}}};
    tree.resize({120.0f, 60.0f});
    tree.activate(platform);

    (void)tree.dispatch(
        test::pointer(ui::InputType::PointerMove, 10.0f, 10.0f), platform);
    NUI_CHECK(log->pointer_moves == 1);
    NUI_CHECK(log->pointer_leaves == 0);

    // T058 may destroy a hovered retained node. The hover route must receive
    // PointerLeave and be cleared while that node is still alive; otherwise the
    // next pointer event would traverse a dangling Node*.
    visible.set(false);
    tree.resize({120.0f, 60.0f});
    NUI_CHECK(log->pointer_leaves == 1);
    NUI_CHECK(log->destructions == 1);

    NUI_CHECK(tree.dispatch(
                  test::pointer(ui::InputType::PointerMove, 10.0f, 10.0f), platform) ==
              ui::EventResult::Ignored);
}

void deterministic_headless_states() {
    {
        ui::State<std::optional<int>> selected{1};
        ui::UI tree{ui::ListView<int>{selected}
            .item(1, ui::Spacer{120.0f, 30.0f})
            .item(2, ui::Spacer{120.0f, 30.0f})};
        ui::HeadlessRenderer renderer{{120.0f, 60.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(pixel_matches(renderer.pixel(8, 15), ui::colors::accent));
        NUI_CHECK(!pixel_matches(renderer.pixel(8, 45), ui::colors::accent));
    }

    {
        ui::State<int> selected{1};
        ui::UI tree{ui::Tabs<int>{selected}
            .tab(1, "One", ui::Spacer{200.0f, 40.0f})
            .tab(2, "Two", ui::Spacer{200.0f, 40.0f})};
        ui::HeadlessRenderer renderer{{200.0f, 100.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(pixel_matches(renderer.pixel(20, 18), ui::colors::input));
        NUI_CHECK(pixel_matches(renderer.pixel(120, 18), ui::colors::panel));

        selected.set(2);
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(pixel_matches(renderer.pixel(20, 18), ui::colors::panel));
        NUI_CHECK(pixel_matches(renderer.pixel(120, 18), ui::colors::input));
    }
}

void suite() {
    t036_contract::behavior_contract();
    fully_retained_large_list_baseline();
    disabled_and_two_instance_contract();
    external_list_selection_reveals_selected_row();
    hover_presentation_contract();
    dynamic_hover_lifetime_contract();
    deterministic_headless_states();
}

} // namespace

int main() { return test::run("t036_list_tabs", &suite); }
