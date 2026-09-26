#include "example_support.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using VirtualState = ui::VirtualListState<int>;

// ---------------------------------------------------------------------------
// Shared demo vocabulary
// ---------------------------------------------------------------------------

ui::Canvas row_view(const VirtualState::Item& item) {
    return ui::Canvas{
        {480.0f, 26.0f},
        [label = item.name, enabled = item.enabled](ui::CanvasContext2D& context) {
            context.text(
                {14.0f, context.height() * 0.5f},
                label,
                12.0f,
                enabled ? ui::colors::text : ui::Color{0.39f, 0.42f, 0.47f, 1.0f},
                ui::TextAlign::Left);
        }};
}

std::vector<VirtualState::Item> make_items(std::size_t count, bool advertise_actions = true) {
    std::vector<VirtualState::Item> items;
    items.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        VirtualState::Item item{
            static_cast<int>(index),
            "Preset " + std::to_string(index + 1),
            index % 257 != 0};
        if (advertise_actions) {
            item.actions = {ui::SemanticAction::Select, ui::SemanticAction::Focus};
        }
        items.push_back(std::move(item));
    }
    return items;
}

ui::Canvas tab_panel(std::string title, std::string body) {
    return ui::Canvas{
        {420.0f, 116.0f},
        [title = std::move(title), body = std::move(body)](ui::CanvasContext2D& context) {
            context.text({16.0f, 26.0f}, title, 16.0f, ui::colors::text, ui::TextAlign::Left);
            context.text(
                {16.0f, 52.0f}, body, 12.0f, ui::colors::textMuted, ui::TextAlign::Left);
            context.fill_rounded_rect({16.0f, 76.0f, 158.0f, 26.0f}, 6.0f, ui::colors::input);
            context.text(
                {95.0f, 89.0f},
                "Component::semantics()",
                11.0f,
                ui::colors::textMuted,
                ui::TextAlign::Center);
        }};
}

// ---------------------------------------------------------------------------
// Custom component using the approved public semantic hook
// ---------------------------------------------------------------------------

/// Application component exercising the frozen T068 custom-component seam: the
/// component owns its semantic projection through `Component::semantics()` and
/// the retained tree exposes it through `UI::component_semantics(NodeId)`.
class SemanticStatusBadge final : public ui::Component {
public:
    SemanticStatusBadge(ui::Binding<std::string> status, std::shared_ptr<ui::NodeId> node_id)
        : status_(std::move(status)), node_id_(std::move(node_id)) {}

    [[nodiscard]] ui::SemanticInfo semantics() const override {
        ui::SemanticInfo info;
        info.role = ui::SemanticRole::Custom;
        info.name = status_.get();
        info.description = "Application status badge semantic projection";
        info.read_only = true;
        return info;
    }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {260.0f, 30.0f};
    }

    void mount(ui::MountContext& context) override {
        if (node_id_) *node_id_ = context.node_id();
    }

    void paint(ui::PaintContext& context) const override {
        const auto bounds = context.bounds();
        auto& painter = context.painter();
        painter.fill_rounded_rect(bounds, 6.0f, ui::colors::input);
        painter.stroke_rounded_rect(bounds, 6.0f, 1.0f, ui::colors::border);
        painter.text(
            {bounds.x + 12.0f, bounds.y + bounds.h * 0.5f},
            "status: " + status_.get(),
            12.0f,
            ui::colors::text,
            ui::TextAlign::Left);
    }

    ui::Spec spec() && {
        auto status = std::move(status_);
        auto node_id = std::move(node_id_);
        return ui::Spec{
            [status = std::move(status), node_id = std::move(node_id)]() mutable {
                return std::make_unique<SemanticStatusBadge>(status, node_id);
            },
            {}};
    }

private:
    ui::Binding<std::string> status_;
    std::shared_ptr<ui::NodeId> node_id_;
};

// ---------------------------------------------------------------------------
// Self-test query host for production widget projections
// ---------------------------------------------------------------------------

/// Self-test-only query host. It owns one production widget component built
/// from that widget's public `Spec` and exposes the production
/// `Component::semantics()` verbatim through a node whose `NodeId` the
/// self-test captures at mount. No widget semantic logic is reimplemented.
class ProjectionHost final : public ui::Component {
public:
    ProjectionHost(std::function<std::unique_ptr<ui::Component>()> factory,
                   std::shared_ptr<ui::NodeId> node_id)
        : component_(factory ? factory() : nullptr), node_id_(std::move(node_id)) {}

    [[nodiscard]] ui::SemanticInfo semantics() const override {
        return component_ ? component_->semantics() : ui::SemanticInfo{};
    }

    [[nodiscard]] bool focusable() const noexcept override {
        return component_ && component_->focusable();
    }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>& children) const override {
        return component_ ? component_->measure(children) : ui::Size{};
    }

    [[nodiscard]] ui::Size minimum_size(
        const std::vector<ui::ChildMetrics>& children) const override {
        return component_ ? component_->minimum_size(children) : ui::Size{};
    }

    void mount(ui::MountContext& context) override {
        if (node_id_) *node_id_ = context.node_id();
    }

    void paint(ui::PaintContext& context) const override {
        if (component_) component_->paint(context);
    }

private:
    std::unique_ptr<ui::Component> component_;
    std::shared_ptr<ui::NodeId> node_id_;
};

class ProjectedControl {
public:
    ProjectedControl(ui::Spec inner, std::shared_ptr<ui::NodeId> node_id)
        : factory_(std::move(inner.factory)), node_id_(std::move(node_id)) {}

    ui::Spec spec() && {
        auto factory = std::move(factory_);
        auto node_id = std::move(node_id_);
        return ui::Spec{
            [factory = std::move(factory), node_id = std::move(node_id)]() mutable {
                return std::make_unique<ProjectionHost>(factory, node_id);
            },
            {}};
    }

private:
    std::function<std::unique_ptr<ui::Component>()> factory_;
    std::shared_ptr<ui::NodeId> node_id_;
};

// ---------------------------------------------------------------------------
// Interactive application
// ---------------------------------------------------------------------------

struct DemoState {
    ui::State<bool> verbose{true};
    ui::State<float> gain{0.5f};
    ui::State<std::string> filter{"preset"};
    ui::State<std::string> status{"Ready"};
    ui::State<int> tab{1};
    ui::State<std::optional<int>> selection{std::nullopt};
    int activations{};
    int activated_key{-1};
    VirtualState list;

    explicit DemoState(
        std::size_t item_count = 100000,
        std::size_t* row_factory_calls = nullptr)
        : list(
              selection,
              26.0f,
              [row_factory_calls](const VirtualState::Item& item) {
                  if (row_factory_calls) ++*row_factory_calls;
                  return row_view(item);
              }) {
        (void)list.replace(make_items(item_count));
    }
};

ui::UI make_ui(DemoState& state, std::shared_ptr<ui::NodeId> badge_id) {
    return ui::UI{
        ui::Column{
            ui::Label{"T068 — Accessibility semantics"}.size(22.0f).bold(),
            ui::Label{
                "Standard controls and a custom component publish Component::semantics(); "
                "100,000 virtual rows share one immutable semantic metadata snapshot."}
                .size(12.0f)
                .color(ui::colors::textMuted),
            ui::Row{
                ui::Button{"Announce", [&state] {
                               ++state.activations;
                               state.status.set("Announced " + std::to_string(state.activations));
                           }},
                ui::Checkbox{state.verbose, "Verbose names"},
                ui::Slider{state.gain}.range(0.0f, 1.0f).step(0.05f)}
                .gap(14.0f),
            ui::Row{
                ui::TextInput{"Filter", state.filter}.placeholder("type to filter presets"),
                SemanticStatusBadge{state.status.binding(), badge_id}}
                .gap(14.0f),
            ui::Tabs<int>{state.tab}
                .tab(
                    1,
                    "Semantics",
                    tab_panel(
                        "Semantic projections",
                        "Button, Checkbox, Slider, Tabs and ListView stay addressable by role, name, state and actions."))
                .tab(
                    2,
                    "Virtual list",
                    tab_panel(
                        "Virtual collection",
                        "Offscreen logical rows resolve without materializing a visual row or copying O(N) metadata.")),
            ui::Flex{
                ui::ListView<int>{state.list}.on_activate([&state](const int& key) {
                    state.activated_key = key;
                })}
                .grow(1.0f)
                .shrink(1.0f),
            ui::Label{
                "↑ ↓ navigate  ·  Home / End  ·  Enter activates  ·  Tab moves focus  ·  wheel scrolls"}
                .size(11.0f)
                .color(ui::colors::textMuted)}
            .gap(12.0f)
            .padding(20.0f)};
}

// ---------------------------------------------------------------------------
// Deterministic headless self-test
// ---------------------------------------------------------------------------

/// Per-node roles/names/actions/state through the public semantic query seam
/// `UI::component_semantics(NodeId)`, covering production standard controls.
int check_ordinary_semantics() {
    example::Platform platform;
    ui::State<bool> checked{true};
    ui::State<float> gain{0.25f};

    auto button_id = std::make_shared<ui::NodeId>(ui::kInvalidNodeId);
    auto checkbox_id = std::make_shared<ui::NodeId>(ui::kInvalidNodeId);
    auto slider_id = std::make_shared<ui::NodeId>(ui::kInvalidNodeId);
    auto label_id = std::make_shared<ui::NodeId>(ui::kInvalidNodeId);

    ui::UI tree{
        ui::Column{
            ProjectedControl{ui::Button{"Save preset", [] {}}.spec(), button_id},
            ProjectedControl{ui::Checkbox{checked, "Verbose"}.spec(), checkbox_id},
            ProjectedControl{ui::Slider{gain}.range(0.0f, 1.0f).step(0.05f).spec(), slider_id},
            ProjectedControl{ui::Label{"Preset 1 of 100000"}.spec(), label_id}}};
    tree.resize({320.0f, 200.0f});
    tree.activate(platform);

    if (*button_id == ui::kInvalidNodeId || *checkbox_id == ui::kInvalidNodeId ||
        *slider_id == ui::kInvalidNodeId || *label_id == ui::kInvalidNodeId) {
        return example::fail("semantic query hosts did not capture their retained NodeId");
    }

    const auto button = tree.component_semantics(*button_id);
    if (!button || button->role != ui::SemanticRole::Button || button->name != "Save preset" ||
        !button->enabled || !button->focusable ||
        !button->supports(ui::SemanticAction::Activate) ||
        !button->supports(ui::SemanticAction::Focus)) {
        return example::fail("Button semantic projection is incomplete");
    }

    const auto checkbox = tree.component_semantics(*checkbox_id);
    if (!checkbox || checkbox->role != ui::SemanticRole::Checkbox ||
        checkbox->name != "Verbose" ||
        checkbox->checked != ui::SemanticCheckedState::Checked ||
        !checkbox->supports(ui::SemanticAction::Toggle) ||
        !checkbox->supports(ui::SemanticAction::Focus)) {
        return example::fail("Checkbox semantic projection is incomplete");
    }
    checked.set(false);
    const auto unchecked = tree.component_semantics(*checkbox_id);
    if (!unchecked || unchecked->checked != ui::SemanticCheckedState::Unchecked) {
        return example::fail("Checkbox checked state did not follow its bound state");
    }

    const auto slider = tree.component_semantics(*slider_id);
    if (!slider || slider->role != ui::SemanticRole::Slider || !slider->numeric_value ||
        !example::near(static_cast<float>(*slider->numeric_value), 0.25f) ||
        !slider->value_range || !example::near(static_cast<float>(slider->value_range->maximum), 1.0f) ||
        !slider->supports(ui::SemanticAction::Increment) ||
        !slider->supports(ui::SemanticAction::Decrement) ||
        !slider->supports(ui::SemanticAction::SetValue) ||
        !slider->supports(ui::SemanticAction::Focus)) {
        return example::fail("Slider numeric value/range projection is incomplete");
    }
    gain.set(0.75f);
    const auto moved = tree.component_semantics(*slider_id);
    if (!moved || !moved->numeric_value ||
        !example::near(static_cast<float>(*moved->numeric_value), 0.75f)) {
        return example::fail("Slider value did not follow its bound state");
    }

    const auto label = tree.component_semantics(*label_id);
    if (!label || label->role != ui::SemanticRole::Text ||
        label->name != "Preset 1 of 100000" || label->focusable ||
        !label->actions.empty()) {
        return example::fail("Label text projection is incomplete");
    }

    if (tree.component_semantics(999999).has_value()) {
        return example::fail("unknown NodeId did not fail closed");
    }

    ui::HeadlessRenderer renderer{{320.0f, 200.0f}, 1.0f};
    if (!renderer.render(tree)) return example::fail("semantic projection render failed");
    return 0;
}

/// The 100k virtual ListView keeps one O(N) immutable metadata allocation and
/// one dataset generation across scroll/selection/focus semantic publications.
/// Full logical reads never invoke the visual row factory.
int check_virtual_metadata_sharing() {
    example::Platform platform;
    ui::State<std::optional<int>> selection{std::nullopt};
    std::size_t row_factory_calls = 0;
    VirtualState state{
        selection,
        20.0f,
        [&row_factory_calls](const VirtualState::Item&) {
            ++row_factory_calls;
            return ui::Spacer{320.0f, 20.0f};
        }};

    if (!state.replace(make_items(100000))) {
        return example::fail("100k dataset replacement failed");
    }
    if (row_factory_calls != 0) {
        return example::fail("dataset replacement materialized visual rows");
    }

    const auto metadata = state.metadata_snapshot();
    const auto generation = state.dataset_generation();
    if (!metadata || metadata->size() != 100000) {
        return example::fail("100k immutable semantic metadata snapshot missing");
    }

    const ui::Rect list_bounds{0.0f, 0.0f, 320.0f, 200.0f};
    const auto initial = state.semantic_children(list_bounds);
    if (initial.size() != 100000 || initial.dataset_generation() != generation ||
        initial.metadata_snapshot().get() != metadata.get()) {
        return example::fail("initial virtual semantic projection is not the shared generation");
    }

    const auto offscreen = initial.item_at(99999);
    if (!offscreen || offscreen->token == ui::kInvalidVirtualSemanticItemToken ||
        offscreen->info.role != ui::SemanticRole::ListItem ||
        offscreen->info.name != "Preset 100000" || !offscreen->info.focusable) {
        return example::fail("full logical child count or offscreen item read failed");
    }
    if (row_factory_calls != 0) {
        return example::fail("offscreen semantic query materialized a visual row");
    }

    ui::UI tree{ui::ListView<int>{state}};
    tree.resize({320.0f, 200.0f});
    tree.activate(platform);
    ui::HeadlessRenderer renderer{{320.0f, 200.0f}, 1.0f};
    if (!renderer.render(tree)) return example::fail("virtual list render failed");
    if (row_factory_calls > 16) {
        return example::fail("initial materialization exceeded the viewport bound");
    }

    const auto before_scroll = row_factory_calls;
    if (!state.scroll_to_index(50000)) {
        return example::fail("scroll_to_index rejected a current logical item");
    }
    const auto after_scroll_calls = row_factory_calls;
    const auto scrolled = state.semantic_children(list_bounds);
    if (scrolled.dataset_generation() != generation ||
        scrolled.metadata_snapshot().get() != metadata.get() ||
        scrolled.size() != 100000) {
        return example::fail("scroll publication rebuilt the immutable virtual metadata");
    }
    if (after_scroll_calls > before_scroll + 16) {
        return example::fail("scroll materialized beyond the viewport window");
    }
    if (row_factory_calls != after_scroll_calls) {
        return example::fail("scroll semantic read materialized a visual row");
    }

    selection.set(99999);
    const auto before_selection_read = row_factory_calls;
    const auto selected = state.semantic_children(list_bounds);
    const auto selected_item = selected.item_at(99999);
    if (selected.dataset_generation() != generation ||
        selected.metadata_snapshot().get() != metadata.get() || !selected_item ||
        !selected_item->info.selected) {
        return example::fail("selection publication rebuilt the immutable virtual metadata");
    }
    if (row_factory_calls != before_selection_read) {
        return example::fail("selection semantic read materialized a visual row");
    }

    const auto before_end = row_factory_calls;
    if (tree.dispatch(example::key(ui::Key::End), platform) != ui::EventResult::Handled) {
        return example::fail("virtual list did not route focused keyboard navigation");
    }
    if (!renderer.render(tree) || row_factory_calls > before_end + 20) {
        return example::fail("focus/scroll materialization exceeded the viewport window");
    }
    const auto after_focus = state.semantic_children(list_bounds);
    const auto focused_item = after_focus.item_at(99999);
    if (after_focus.dataset_generation() != generation ||
        after_focus.metadata_snapshot().get() != metadata.get() || !focused_item ||
        !focused_item->info.selected) {
        return example::fail("scroll/selection/focus publications rebuilt virtual metadata");
    }
    const std::size_t after_end_render = row_factory_calls;

    // A genuine dataset replacement publishes exactly one new immutable
    // generation; an already-retained old snapshot stays readable.
    std::vector<VirtualState::Item> replaced = make_items(100000);
    replaced[99999].name = "Preset updated";
    if (!state.replace(std::move(replaced))) {
        return example::fail("100k metadata replacement failed");
    }
    const auto updated = state.metadata_snapshot();
    if (state.dataset_generation() != generation + 1 || !updated ||
        updated.get() == metadata.get() || metadata->back().name != "Preset 100000" ||
        updated->back().name != "Preset updated" || row_factory_calls != after_end_render) {
        return example::fail("dataset replacement did not swap one immutable generation");
    }
    return 0;
}

/// Virtual logical Select/Focus behavior at the public user level. T045
/// deliberately keeps semantic-action dispatch internal to the platform
/// bridge, so this exercises the public equivalents: logical token resolution
/// against the current immutable metadata, the application-owned selection
/// state and the retained focus/input path. Logical resolution revalidates
/// current metadata (stability, removal, enabled/read-only) and never
/// materializes a visual row; only the ordinary retained layout may.
int check_virtual_logical_actions() {
    example::Platform platform;
    ui::State<std::optional<int>> selection{std::nullopt};
    std::size_t row_factory_calls = 0;
    VirtualState state{
        selection,
        20.0f,
        [&row_factory_calls](const VirtualState::Item&) {
            ++row_factory_calls;
            return ui::Spacer{240.0f, 20.0f};
        }};

    const auto item = [](int key, std::string name, bool enabled, bool read_only = false) {
        VirtualState::Item value{key, std::move(name), enabled};
        value.read_only = read_only;
        value.actions = {ui::SemanticAction::Select, ui::SemanticAction::Focus};
        return value;
    };

    const ui::Rect list_bounds{0.0f, 0.0f, 240.0f, 60.0f};
    if (!state.replace({
            item(10, "Ten", true),
            item(20, "Twenty", false),
            item(30, "Thirty", true),
        })) {
        return example::fail("virtual dataset replacement failed");
    }

    auto current = state.semantic_children(list_bounds);
    const auto thirty = current.item_at(2);
    if (!thirty) return example::fail("logical item read failed");
    const auto thirty_token = thirty->token;

    // Reorder keeps the stable logical token and resolves the new index.
    if (!state.replace({
            item(30, "Thirty", true),
            item(10, "Ten", true),
            item(20, "Twenty", false),
        })) {
        return example::fail("virtual dataset reorder failed");
    }
    current = state.semantic_children(list_bounds);
    const auto resolved = current.item_for_token(thirty_token);
    if (!resolved || resolved->info.name != "Thirty" || resolved->info.selected) {
        return example::fail("stable logical token did not follow the reordered item");
    }

    // Public Select: the application owns selection; the semantic projection
    // resolves selected state against the current dataset.
    if (!resolved->info.supports(ui::SemanticAction::Select)) {
        return example::fail("current item no longer advertises Select");
    }
    selection.set(30);
    current = state.semantic_children(list_bounds);
    const auto reselected = current.item_at(0);
    if (!reselected || !reselected->info.selected ||
        current.index_of_selected_item() != 0 || row_factory_calls != 0) {
        return example::fail("selection did not revalidate against the reordered dataset");
    }

    // Removing the logical key makes its token stale: identity and selection
    // fail closed instead of redirecting to a replacement row.
    if (!state.replace({
            item(10, "Ten", true),
            item(40, "Forty", true),
        })) {
        return example::fail("virtual dataset removal failed");
    }
    current = state.semantic_children(list_bounds);
    if (current.item_for_token(thirty_token)) {
        return example::fail("removed logical token still resolved");
    }
    for (std::size_t index = 0; index < current.size(); ++index) {
        const auto entry = current.item_at(index);
        if (entry && entry->info.selected) {
            return example::fail("stale selection marked a replacement row selected");
        }
    }

    // The retained focus/input path is the public Focus action equivalent: the
    // virtual ListView holds focus and commits only enabled logical rows.
    if (!state.replace({
            item(10, "Ten", true, true),
            item(20, "Twenty", false),
            item(30, "Thirty", true),
        })) {
        return example::fail("virtual dataset restore failed");
    }
    const auto restored = state.semantic_children(list_bounds);
    const auto read_only_item = restored.item_at(0);
    if (!read_only_item || !read_only_item->info.read_only ||
        !read_only_item->info.supports(ui::SemanticAction::Select) ||
        !read_only_item->info.supports(ui::SemanticAction::Focus)) {
        return example::fail("read-only logical policy is not exposed");
    }
    selection.set(std::nullopt);
    ui::UI tree{ui::ListView<int>{state}};
    tree.resize({240.0f, 60.0f});
    tree.activate(platform);
    // The viewport materializes the three logical rows (the whole dataset).
    const std::size_t materialized_rows = row_factory_calls;
    if (materialized_rows > 3) {
        return example::fail("small virtual dataset materialized beyond the viewport");
    }
    if (tree.dispatch(example::key(ui::Key::Down), platform) != ui::EventResult::Handled) {
        return example::fail("virtual list did not route focused keyboard input");
    }
    if (!selection.get() || *selection.get() != 30) {
        return example::fail("Down did not skip the disabled logical row");
    }
    const auto shared = state.metadata_snapshot();
    const auto generation = state.dataset_generation();
    const auto after_input = state.semantic_children(list_bounds);
    if (after_input.metadata_snapshot().get() != shared.get() ||
        after_input.dataset_generation() != generation) {
        return example::fail("logical selection publication rebuilt immutable metadata");
    }
    if (row_factory_calls != materialized_rows) {
        return example::fail("logical keyboard selection materialized extra visual rows");
    }
    return 0;
}

/// The composed interactive layout also runs deterministically headless: the
/// custom semantics override is surfaced through the public per-node query and
/// the virtual list materializes only a viewport-bounded number of rows.
int check_interactive_startup() {
    example::Platform platform;
    std::size_t row_factory_calls = 0;
    DemoState state{1000, &row_factory_calls};
    auto badge_id = std::make_shared<ui::NodeId>(ui::kInvalidNodeId);
    auto tree = make_ui(state, badge_id);
    tree.resize({720.0f, 640.0f});
    tree.activate(platform);

    if (*badge_id == ui::kInvalidNodeId) {
        return example::fail("custom component did not publish its retained NodeId");
    }
    const auto badge = tree.component_semantics(*badge_id);
    if (!badge || badge->role != ui::SemanticRole::Custom || badge->name != "Ready" ||
        badge->description.empty() || !badge->read_only) {
        return example::fail("custom component semantics() override is not exposed");
    }
    state.status.set("Interaction ready");
    const auto updated = tree.component_semantics(*badge_id);
    if (!updated || updated->name != "Interaction ready") {
        return example::fail("custom component semantics() did not follow live state");
    }

    ui::HeadlessRenderer renderer{{720.0f, 640.0f}, 1.0f};
    if (!renderer.render(tree)) return example::fail("interactive example render failed");
    if (row_factory_calls > 32) {
        return example::fail("interactive startup materialization exceeded the viewport bound");
    }
    return 0;
}

int self_test() {
    if (const int result = check_ordinary_semantics()) return result;
    if (const int result = check_virtual_metadata_sharing()) return result;
    if (const int result = check_virtual_logical_actions()) return result;
    if (const int result = check_interactive_startup()) return result;
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    DemoState state;
    auto badge_id = std::make_shared<ui::NodeId>(ui::kInvalidNodeId);
    auto tree = make_ui(state, badge_id);
    return example::run_window(tree, "NativeUI T068 Accessibility Semantics", {720.0f, 640.0f});
}
