#include "test_support.hpp"
#include <nativeui/grid.hpp>

#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

struct CellObservation {
    ui::Size minimum{10.0f, 10.0f};
    ui::Size preferred{20.0f, 20.0f};
    std::vector<ui::Rect> focus_bounds;
    ui::Rect painted_bounds{};
    bool throw_measure{};
    bool wrap_at_50{};
    std::vector<float> measured_widths;
};

class CellComponent final : public ui::Component {
public:
    explicit CellComponent(std::shared_ptr<CellObservation> state) : state_(std::move(state)) {}
    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        if (state_->throw_measure) throw std::runtime_error("span measure failure");
        return state_->preferred;
    }
    [[nodiscard]] ui::Size minimum_size(const std::vector<ui::ChildMetrics>&) const override {
        return state_->minimum;
    }
    [[nodiscard]] ui::ChildMetrics measure_constrained(
        const ui::Constraints& constraints, const std::vector<ui::ChildMetrics>& children) const override {
        state_->measured_widths.push_back(constraints.max.w);
        auto metrics = ui::Component::measure_constrained(constraints, children);
        if (state_->wrap_at_50) {
            metrics.preferred.h = constraints.max.w < 50.0f ? 40.0f : 20.0f;
        }
        return metrics;
    }
    void focus_changed(bool focused, ui::FocusContext& context) override {
        if (focused) state_->focus_bounds.push_back(context.bounds());
    }
    void paint(ui::PaintContext& context) const override {
        state_->painted_bounds = context.bounds();
    }
private:
    std::shared_ptr<CellObservation> state_;
};

class Cell {
public:
    explicit Cell(std::shared_ptr<CellObservation> state) : state_(std::move(state)) {}
    ui::Spec spec() && {
        auto state = state_;
        return ui::Spec{[state] { return std::make_unique<CellComponent>(state); }, {}};
    }
private:
    std::shared_ptr<CellObservation> state_;
};

void check_rect(ui::Rect actual, ui::Rect expected) {
    NUI_CHECK_NEAR(actual.x, expected.x, 0.001f);
    NUI_CHECK_NEAR(actual.y, expected.y, 0.001f);
    NUI_CHECK_NEAR(actual.w, expected.w, 0.001f);
    NUI_CHECK_NEAR(actual.h, expected.h, 0.001f);
}

void explicit_cells_reserve_space_before_auto_placement() {
    auto a = std::make_shared<CellObservation>();
    auto b = std::make_shared<CellObservation>();
    auto c = std::make_shared<CellObservation>();
    ui::UI tree{ui::Grid{
        ui::GridTracks{.columns={ui::Track::fixed(50.0f), ui::Track::fixed(50.0f)},
                       .rows={ui::Track::auto_size()}}, Cell{a}, Cell{b}}
        .cell({0, 0, 1, 1}, Cell{c}).gap(10.0f)};
    test::MockPlatform platform;
    tree.resize({110.0f, 80.0f});
    tree.activate(platform);
    check_rect(a->focus_bounds.back(), {60.0f, 0.0f, 50.0f, 20.0f});
    tree.dispatch(test::key(ui::Key::Tab), platform);
    check_rect(b->focus_bounds.back(), {0.0f, 30.0f, 50.0f, 20.0f});
    tree.dispatch(test::key(ui::Key::Tab), platform);
    check_rect(c->focus_bounds.back(), {0.0f, 0.0f, 50.0f, 20.0f});
}

void span_intrinsics_leave_fixed_tracks_unchanged_and_respect_minimums() {
    auto span = std::make_shared<CellObservation>();
    span->minimum = {60.0f, 12.0f};
    span->preferred = {240.0f, 30.0f};
    ui::UI tree{ui::Grid{ui::GridTracks{
        .columns={ui::Track::fixed(80.0f), ui::Track::auto_size(), ui::Track::flex()},
        .rows={ui::Track::auto_size()}}}
        .cell({0, 0, 1, 3}, Cell{span}).column_gap(10.0f)};
    const auto measured = tree.measure();
    NUI_CHECK_NEAR(measured.minimum.w, 100.0f, 0.001f);
    NUI_CHECK_NEAR(measured.preferred.w, 240.0f, 0.001f);
    test::MockPlatform platform;
    tree.resize({360.0f, 60.0f});
    tree.activate(platform);
    check_rect(span->focus_bounds.back(), {0.0f, 0.0f, 360.0f, 30.0f});
    tree.resize({80.0f, 60.0f});
    ui::HeadlessRenderer renderer{{80.0f, 60.0f}, 1.0f};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK_NEAR(span->painted_bounds.w, 100.0f, 0.001f);
}

void spans_can_extend_initial_tracks_and_rows() {
    auto span = std::make_shared<CellObservation>();
    span->minimum = {100.0f, 20.0f};
    span->preferred = {100.0f, 20.0f};
    ui::UI tree{ui::Grid{ui::GridTracks{
        .columns={ui::Track::fixed(30.0f)}, .rows={ui::Track::fixed(10.0f)}}}
        .cell({1, 1, 1, 2}, Cell{span}).gap(5.0f)};
    NUI_CHECK_NEAR(tree.measure().preferred.w, 135.0f, 0.001f);
    NUI_CHECK_NEAR(tree.measure().preferred.h, 35.0f, 0.001f);
    test::MockPlatform platform;
    tree.resize({200.0f, 80.0f});
    tree.activate(platform);
    check_rect(span->focus_bounds.back(), {35.0f, 15.0f, 100.0f, 20.0f});
}

void invalid_cells_throw_before_spec_consumption_and_next_grid_recovers() {
    auto invalid = ui::Grid{ui::GridTracks{}}
        .cell({0,0,1,1}, ui::Spacer{10.0f, 10.0f})
        .cell({0,0,1,1}, ui::Spacer{10.0f, 10.0f});
    for (int retry = 0; retry < 2; ++retry) {
        bool threw = false;
        try { (void)std::move(invalid).spec(); }
        catch (const std::invalid_argument&) { threw = true; }
        NUI_CHECK(threw);
    }
    for (auto invalid_cell : {
            ui::GridCell{0,0,0,1},
            ui::GridCell{std::numeric_limits<std::size_t>::max(),0,1,1},
            ui::GridCell{0,std::numeric_limits<std::size_t>::max(),1,1}}) {
        bool threw = false;
        try {
            (void)ui::Grid{ui::GridTracks{}}
                .cell(invalid_cell, ui::Spacer{10.0f, 10.0f}).spec();
        } catch (const std::invalid_argument&) { threw = true; }
        NUI_CHECK(threw);
    }
    ui::State<ui::VisibilityMode> hidden{ui::VisibilityMode::Collapsed};
    bool structural_overlap_threw = false;
    try {
        (void)ui::Grid{ui::GridTracks{}}
            .cell({0,0,1,2}, ui::Visibility{hidden.binding(), ui::Spacer{10.0f,10.0f}})
            .cell({0,1,1,1}, ui::Spacer{10.0f,10.0f}).spec();
    } catch (const std::invalid_argument&) { structural_overlap_threw = true; }
    NUI_CHECK(structural_overlap_threw);
    bool extent_threw = false;
    try {
        (void)ui::Grid{ui::GridTracks{
            .columns={ui::Track::fixed(std::numeric_limits<float>::max()),
                      ui::Track::fixed(std::numeric_limits<float>::max())}, .rows={}}}.spec();
    } catch (const std::invalid_argument&) { extent_threw = true; }
    NUI_CHECK(extent_threw);
    ui::UI valid{ui::Grid{ui::GridTracks{}}.cell({0,0,1,1}, ui::Spacer{20.0f,30.0f})};
    NUI_CHECK_NEAR(valid.measure().preferred.w, 20.0f, 0.001f);
}

void failed_child_measure_keeps_layout_recoverable_and_other_instance_works() {
    auto cell = std::make_shared<CellObservation>();
    ui::UI first{ui::Grid{ui::GridTracks{.columns={ui::Track::flex()}, .rows={}}}
        .cell({0,0,1,1}, Cell{cell})};
    ui::UI second{ui::Grid{ui::GridTracks{}}.cell({0,0,1,1}, ui::Spacer{25.0f,35.0f})};
    first.resize({100.0f,80.0f});
    cell->throw_measure = true;
    bool threw = false;
    try { first.resize({120.0f,90.0f}); }
    catch (const std::runtime_error&) { threw = true; }
    NUI_CHECK(threw);
    NUI_CHECK(first.layout_dirty());
    NUI_CHECK_NEAR(second.measure().preferred.w, 25.0f, 0.001f);
    cell->throw_measure = false;
    first.resize({120.0f,90.0f});
    test::MockPlatform platform;
    first.activate(platform);
    check_rect(cell->focus_bounds.back(), {0.0f,0.0f,120.0f,20.0f});
    ui::HeadlessRenderer renderer{{120.0f,90.0f}, 1.0f};
    NUI_CHECK(renderer.render(first));
}

void collapsed_cells_release_auto_placement_without_changing_declared_tracks() {
    ui::State<ui::VisibilityMode> auto_mode{ui::VisibilityMode::Collapsed};
    ui::State<ui::VisibilityMode> explicit_mode{ui::VisibilityMode::Collapsed};
    auto visible = std::make_shared<CellObservation>();
    visible->minimum = {0.0f, 0.0f};
    visible->preferred = {90.0f, 20.0f};
    visible->wrap_at_50 = true;
    ui::UI tree{ui::Grid{ui::GridTracks{
        .columns={ui::Track::fixed(20.0f), ui::Track::fixed(100.0f)},
        .rows={ui::Track::auto_size()}},
        ui::Visibility{auto_mode.binding(), ui::Spacer{90.0f, 40.0f}}, Cell{visible}}
        .cell({0,0,1,1}, ui::Visibility{explicit_mode.binding(), ui::Spacer{50.0f,50.0f}})
        .gap(5.0f)};
    test::MockPlatform platform;
    tree.resize({125.0f, 120.0f});
    tree.activate(platform);
    check_rect(visible->focus_bounds.back(), {0.0f, 0.0f, 20.0f, 40.0f});
    NUI_CHECK_NEAR(visible->measured_widths.back(), 20.0f, 0.001f);
    NUI_CHECK_NEAR(tree.measure().preferred.w, 125.0f, 0.001f);
    NUI_CHECK_NEAR(tree.measure().preferred.h, 40.0f, 0.001f);
    explicit_mode.set(ui::VisibilityMode::Visible);
    ui::HeadlessRenderer renderer{{125.0f,120.0f},1.0f};
    NUI_CHECK(renderer.render(tree));
    check_rect(visible->painted_bounds, {25.0f, 0.0f, 100.0f, 50.0f});
    auto_mode.set(ui::VisibilityMode::Visible);
    NUI_CHECK(renderer.render(tree));
    check_rect(visible->painted_bounds, {0.0f, 55.0f, 20.0f, 40.0f});
    // In a smaller viewport, the first row's Spacer minimum of 50 cannot
    // shrink. The second Auto row contributes the remaining 25 (80-50-5).
    tree.resize({125.0f,80.0f});
    ui::HeadlessRenderer compact_renderer{{125.0f,80.0f},1.0f};
    NUI_CHECK(compact_renderer.render(tree));
    check_rect(visible->painted_bounds, {0.0f,55.0f,20.0f,25.0f});
    tree.resize({125.0f,120.0f});
    NUI_CHECK(renderer.render(tree));
    check_rect(visible->painted_bounds, {0.0f,55.0f,20.0f,40.0f});
    explicit_mode.set(ui::VisibilityMode::Collapsed);
    auto_mode.set(ui::VisibilityMode::Collapsed);
    NUI_CHECK(renderer.render(tree));
    check_rect(visible->painted_bounds, {0.0f,0.0f,20.0f,40.0f});
}

void empty_and_zero_flex_tracks_resize_with_finite_geometry() {
    ui::UI empty{ui::Grid{ui::GridTracks{}}};
    NUI_CHECK_NEAR(empty.measure().preferred.w, 0.0f, 0.001f);
    NUI_CHECK_NEAR(empty.measure().preferred.h, 0.0f, 0.001f);
    empty.resize({0.0f, 0.0f});
    empty.resize({150.0f, 90.0f});
    ui::HeadlessRenderer empty_renderer{{150.0f,90.0f},1.0f};
    NUI_CHECK(empty_renderer.render(empty));
    auto span = std::make_shared<CellObservation>();
    span->minimum = {10.0f,10.0f};
    span->preferred = {60.0f,20.0f};
    ui::UI tree{ui::Grid{ui::GridTracks{
        .columns={ui::Track::flex(0.0f), ui::Track::auto_size()}, .rows={}}}
        .cell({0,0,1,2}, Cell{span}).gap(5.0f)};
    tree.resize({0.0f,0.0f});
    tree.resize({150.0f,90.0f});
    test::MockPlatform platform;
    tree.activate(platform);
    check_rect(span->focus_bounds.back(), {0.0f,0.0f,60.0f,20.0f});
}

void sparse_reservations_do_not_require_a_cell_matrix_during_constraints() {
    // Two adjacent rectangles cover a billion complete rows. Constraint
    // planning jumps past their common end instead of scanning every cell.
    const std::size_t many_rows = 1'000'000'000;
    ui::GridComponent grid{
        ui::GridTracks{.columns={ui::Track::auto_size(), ui::Track::auto_size()}, .rows={}},
        1.0f, 1.0f,
        {ui::GridCell{0,0,many_rows,1}, ui::GridCell{0,1,many_rows,1}, std::nullopt}};
    const std::vector<ui::ChildMetrics> metadata(3);
    const auto constraints = grid.child_constraints(ui::Constraints::loose({100.0f,100.0f}), 2, metadata);
    NUI_CHECK_NEAR(constraints.max.w, 100.0f, 0.001f);
    NUI_CHECK_NEAR(constraints.max.h, 100.0f, 0.001f);
}

void historic_count_constraint_hook_preserves_all_children_as_participants() {
    ui::GridComponent grid{
        ui::GridTracks{.columns={ui::Track::fixed(30.0f),ui::Track::flex()},
                       .rows={ui::Track::fixed(10.0f)}},5.0f,5.0f};
    std::vector<ui::ChildMetrics> metadata(3);
    const auto parent = ui::Constraints::loose({100.0f,200.0f});
    for (std::size_t i = 0; i < metadata.size(); ++i) {
        const auto historic = grid.child_constraints(parent,i,metadata.size());
        const auto current = grid.child_constraints(parent,i,metadata);
        NUI_CHECK_NEAR(historic.max.w,current.max.w,0.001f);
        NUI_CHECK_NEAR(historic.max.h,current.max.h,0.001f);
    }
    NUI_CHECK_NEAR(grid.child_constraints(parent,1,metadata.size()).max.w,100.0f,0.001f);
    metadata[0].participates_in_layout = false;
    NUI_CHECK_NEAR(grid.child_constraints(parent,1,metadata).max.w,30.0f,0.001f);
    // The old public overload does not inherit a previous metadata snapshot.
    NUI_CHECK_NEAR(grid.child_constraints(parent,1,metadata.size()).max.w,100.0f,0.001f);
}

void suite() {
    explicit_cells_reserve_space_before_auto_placement();
    span_intrinsics_leave_fixed_tracks_unchanged_and_respect_minimums();
    spans_can_extend_initial_tracks_and_rows();
    invalid_cells_throw_before_spec_consumption_and_next_grid_recovers();
    failed_child_measure_keeps_layout_recoverable_and_other_instance_works();
    collapsed_cells_release_auto_placement_without_changing_declared_tracks();
    empty_and_zero_flex_tracks_resize_with_finite_geometry();
    sparse_reservations_do_not_require_a_cell_matrix_during_constraints();
    historic_count_constraint_hook_preserves_all_children_as_participants();
}

} // namespace

int main() { return test::run("widget_grid_spans", &suite); }
