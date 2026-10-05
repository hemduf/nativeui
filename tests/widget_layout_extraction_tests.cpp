#include "test_support.hpp"

#include <nativeui/enabled.hpp>
#include <nativeui/padding.hpp>
#include <nativeui/read_only.hpp>
#include <nativeui/visibility.hpp>

#include <limits>
#include <memory>
#include <stdexcept>

namespace {

void binding_wrappers_disconnect_after_model_and_tree_teardown() {
    auto visible = std::make_unique<ui::State<bool>>(false);
    auto mode = std::make_unique<ui::State<ui::VisibilityMode>>(ui::VisibilityMode::Visible);
    auto allowed = std::make_unique<ui::State<bool>>(true);
    auto locked = std::make_unique<ui::State<bool>>(true);
    auto visible_binding = visible->binding();
    auto mode_binding = mode->binding();
    auto allowed_binding = allowed->binding();
    auto locked_binding = locked->binding();

    // A delayed factory remains readable after its application-owned State dies.
    auto boolean_spec = ui::make_spec(
        ui::Visibility{visible_binding, ui::Spacer{10.0f, 20.0f}}
            .mode(ui::VisibilityMode::Visible));
    auto mode_spec = ui::make_spec(ui::Visibility{mode_binding, ui::Spacer{10.0f, 20.0f}});
    auto enabled_spec = ui::make_spec(ui::Enabled{allowed_binding, ui::Spacer{10.0f, 20.0f}});
    auto readonly_spec = ui::make_spec(ui::ReadOnly{locked_binding, ui::Spacer{10.0f, 20.0f}});
    visible.reset();
    mode.reset();
    allowed.reset();
    locked.reset();
    NUI_CHECK(!visible_binding.valid());
    NUI_CHECK(!mode_binding.valid());
    NUI_CHECK(!allowed_binding.valid());
    NUI_CHECK(!locked_binding.valid());
    NUI_CHECK(boolean_spec.factory()->local_availability().visibility == ui::VisibilityMode::Hidden);
    NUI_CHECK(mode_spec.factory()->local_availability().visibility == ui::VisibilityMode::Visible);
    NUI_CHECK(enabled_spec.factory()->local_availability().enabled);
    NUI_CHECK(readonly_spec.factory()->local_availability().read_only);

    test::MockPlatform platform;
    ui::UI tree{std::move(mode_spec)};
    tree.resize({100.0f, 40.0f});
    tree.activate(platform);
    ui::HeadlessRenderer renderer{{100.0f, 40.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
}

void binding_enabled_recovers_armed_button_and_instances_are_independent() {
    ui::State<bool> allowed{true};
    ui::State<bool> other_allowed{true};
    int activations = 0;
    int other_activations = 0;
    auto first = std::make_unique<ui::UI>(
        ui::Enabled{allowed.binding(), ui::Button{"Act", [&] { ++activations; }}});
    ui::UI second{
        ui::Enabled{other_allowed.binding(), ui::Button{"Act", [&] { ++other_activations; }}}};
    test::MockPlatform first_platform;
    test::MockPlatform second_platform;
    first->resize({140.0f, 40.0f});
    second.resize({140.0f, 40.0f});
    first->activate(first_platform);
    second.activate(second_platform);
    first->dispatch(test::pointer(ui::InputType::PointerDown, 10.0f, 10.0f), first_platform);
    allowed.set(false);
    allowed.set(true);
    first->dispatch(test::pointer(ui::InputType::PointerUp, 10.0f, 10.0f), first_platform);
    NUI_CHECK(activations == 0);
    second.dispatch(test::pointer(ui::InputType::PointerDown, 10.0f, 10.0f), second_platform);
    second.dispatch(test::pointer(ui::InputType::PointerUp, 10.0f, 10.0f), second_platform);
    NUI_CHECK(other_activations == 1);
    first.reset();
    allowed.set(false); // The retired availability observer must be disconnected.
    second.dispatch(test::pointer(ui::InputType::PointerDown, 10.0f, 10.0f), second_platform);
    second.dispatch(test::pointer(ui::InputType::PointerUp, 10.0f, 10.0f), second_platform);
    NUI_CHECK(other_activations == 2);
}

void binding_readonly_keeps_external_updates_and_restores_editing() {
    ui::State<bool> locked{true};
    ui::State<std::string> text{"initial"};
    test::MockPlatform platform;
    ui::UI tree{ui::ReadOnly{locked.binding(), ui::TextInput{"", text}}};
    tree.resize({180.0f, 36.0f});
    tree.activate(platform);
    tree.dispatch(test::text("x"), platform);
    NUI_CHECK(text.get() == "initial");
    text.set("external");
    tree.dispatch(test::text("x"), platform);
    NUI_CHECK(text.get() == "external");
    locked.set(false);
    tree.dispatch(test::key(ui::Key::End), platform);
    tree.dispatch(test::text("x"), platform);
    NUI_CHECK(text.get() == "externalx");
}

void binding_visibility_keeps_hidden_space_and_removes_collapsed_space() {
    ui::State<ui::VisibilityMode> mode{ui::VisibilityMode::Visible};
    ui::UI tree{ui::Column{
        ui::Visibility{mode.binding(), ui::Spacer{40.0f, 30.0f}},
        ui::Spacer{40.0f, 20.0f}}.padding(0.0f).gap(10.0f)};
    NUI_CHECK_NEAR(tree.measure().preferred.h, 60.0f, 0.001f);
    mode.set(ui::VisibilityMode::Hidden);
    NUI_CHECK_NEAR(tree.measure().preferred.h, 60.0f, 0.001f);
    mode.set(ui::VisibilityMode::Collapsed);
    NUI_CHECK_NEAR(tree.measure().preferred.h, 20.0f, 0.001f);
    mode.set(ui::VisibilityMode::Visible);
    NUI_CHECK_NEAR(tree.measure().preferred.h, 60.0f, 0.001f);
}

void flow_omits_collapsed_children_but_preserves_zero_sized_spacers() {
    ui::State<ui::VisibilityMode> mode{ui::VisibilityMode::Collapsed};
    ui::UI row{ui::Row{ui::Spacer{10.0f, 10.0f},
                       ui::Visibility{mode.binding(), ui::Spacer{80.0f, 80.0f}},
                       ui::Spacer{20.0f, 20.0f}}.gap(5.0f)};
    ui::UI column{ui::Column{ui::Spacer{10.0f, 10.0f},
                             ui::Visibility{mode.binding(), ui::Spacer{80.0f, 80.0f}},
                             ui::Spacer{20.0f, 20.0f}}.padding(0.0f).gap(5.0f)};
    NUI_CHECK_NEAR(row.measure().preferred.w, 35.0f, 0.001f);
    NUI_CHECK_NEAR(column.measure().preferred.h, 35.0f, 0.001f);
    ui::UI zero_row{ui::Row{ui::Spacer{10.0f, 10.0f}, ui::Spacer{0.0f, 0.0f},
                            ui::Spacer{20.0f, 20.0f}}.gap(5.0f)};
    ui::UI zero_column{ui::Column{ui::Spacer{10.0f, 10.0f}, ui::Spacer{0.0f, 0.0f},
                                  ui::Spacer{20.0f, 20.0f}}.padding(0.0f).gap(5.0f)};
    NUI_CHECK_NEAR(zero_row.measure().preferred.w, 40.0f, 0.001f);
    NUI_CHECK_NEAR(zero_column.measure().preferred.h, 40.0f, 0.001f);
    ui::UI empty{ui::Row{ui::Visibility{mode.binding(), ui::Spacer{10.0f, 10.0f}},
                          ui::Visibility{mode.binding(), ui::Spacer{20.0f, 20.0f}}}.gap(5.0f)};
    NUI_CHECK_NEAR(empty.measure().preferred.w, 0.0f, 0.001f);

    // Inactive flex factors must not steal either growth or spacing. Keep
    // placement indices aligned with the retained children, including the gap.
    auto collapsed = ui::ChildMetrics{{90.0f, 90.0f}, {100.0f, 100.0f}, {100.0f, 1.0f}};
    collapsed.participates_in_layout = false;
    const std::vector<ui::ChildMetrics> metrics{
        ui::ChildMetrics{{10.0f, 10.0f}, {20.0f, 20.0f}, {1.0f, 1.0f}}, collapsed,
        ui::ChildMetrics{{10.0f, 10.0f}, {20.0f, 20.0f}, {1.0f, 1.0f}}};
    ui::RowComponent horizontal{10.0f, ui::Align::Stretch, ui::Justify::SpaceBetween};
    ui::ColumnComponent vertical{10.0f, 0.0f, ui::Align::Stretch, ui::Justify::SpaceBetween};
    std::vector<ui::ChildPlacement> placements(metrics.size());
    horizontal.layout_children({0.0f, 0.0f, 110.0f, 40.0f}, metrics, placements);
    NUI_CHECK_NEAR(placements[0].bounds.w, 50.0f, 0.001f);
    NUI_CHECK_NEAR(placements[2].bounds.x, 60.0f, 0.001f);
    NUI_CHECK(placements[1].bounds.w == 0.0f && placements[1].bounds.h == 0.0f);
    vertical.layout_children({0.0f, 0.0f, 40.0f, 110.0f}, metrics, placements);
    NUI_CHECK_NEAR(placements[0].bounds.h, 50.0f, 0.001f);
    NUI_CHECK_NEAR(placements[2].bounds.y, 60.0f, 0.001f);
    horizontal.layout_children({0.0f, 0.0f, 30.0f, 40.0f}, metrics, placements);
    NUI_CHECK_NEAR(placements[0].bounds.w, 10.0f, 0.001f);
    NUI_CHECK_NEAR(placements[2].bounds.x, 20.0f, 0.001f);
}

void finite_padding_normalizes_each_invalid_input_before_layout() {
    const std::vector<ui::ChildMetrics> metrics{
        ui::ChildMetrics{{12.0f, 8.0f}, {40.0f, 20.0f}}};
    for (float padding : {std::numeric_limits<float>::infinity(),
                          -std::numeric_limits<float>::infinity(),
                          std::numeric_limits<float>::quiet_NaN(), -2.0f}) {
        ui::PaddingComponent component{padding};
        const auto preferred = component.measure(metrics);
        const auto minimum = component.minimum_size(metrics);
        NUI_CHECK_NEAR(preferred.w, 40.0f, 0.001f);
        NUI_CHECK_NEAR(preferred.h, 20.0f, 0.001f);
        NUI_CHECK_NEAR(minimum.w, 12.0f, 0.001f);
        std::vector<ui::ChildPlacement> placements(1);
        component.layout_children({3.0f, 4.0f, 90.0f, 60.0f}, metrics, placements);
        NUI_CHECK_NEAR(placements.front().bounds.x, 3.0f, 0.001f);
        NUI_CHECK_NEAR(placements.front().bounds.w, 90.0f, 0.001f);
        const auto constraints = component.child_constraints(
            ui::Constraints::tight({90.0f, 60.0f}), 0, 1);
        NUI_CHECK_NEAR(constraints.max.w, 90.0f, 0.001f);
    }
    ui::PaddingComponent excessive{50.0f};
    std::vector<ui::ChildPlacement> placements(1);
    excessive.layout_children({0.0f, 0.0f, 40.0f, 30.0f}, metrics, placements);
    NUI_CHECK(placements.front().bounds.w == 0.0f);
    NUI_CHECK(placements.front().bounds.h == 0.0f);
}

void suite() {
    binding_wrappers_disconnect_after_model_and_tree_teardown();
    binding_enabled_recovers_armed_button_and_instances_are_independent();
    binding_readonly_keeps_external_updates_and_restores_editing();
    binding_visibility_keeps_hidden_space_and_removes_collapsed_space();
    flow_omits_collapsed_children_but_preserves_zero_sized_spacers();
    finite_padding_normalizes_each_invalid_input_before_layout();
}

} // namespace

int main() { return test::run("widget_layout_extraction", &suite); }
