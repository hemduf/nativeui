#include "test_support.hpp"

#include <nativeui/detail/raster_cache_access.hpp>

#include "include/core/SkCanvas.h"

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ui {

struct TreeTestAccess {
    [[nodiscard]] static std::size_t raster_records(const Tree& tree) noexcept {
        return tree.raster_cache_epochs_.size();
    }
};

} // namespace ui

namespace ui::detail {

struct CachedLayerTestAccess {
    static void fail_setup_after(Tree& tree, std::size_t checkpoint) noexcept {
        tree.cached_layer_setup_failure_after_ = checkpoint;
        tree.cached_layer_setup_checkpoints_ = 0;
    }

    [[nodiscard]] static std::size_t setup_checkpoints(const Tree& tree) noexcept {
        return tree.cached_layer_setup_checkpoints_;
    }

    [[nodiscard]] static bool setup_fault_armed(const Tree& tree) noexcept {
        return tree.cached_layer_setup_failure_after_.has_value();
    }

    [[nodiscard]] static std::size_t subscriptions(const Tree& tree, NodeId id) {
        return tree.raster_cache_epochs_.at(id)->subscriptions.size();
    }

    [[nodiscard]] static std::size_t layers(const Tree& tree, NodeId id) {
        std::size_t count{};
        for (auto* layer = tree.raster_cache_epochs_.at(id).get(); layer;
             layer = layer->inner.get()) ++count;
        return count;
    }

    [[nodiscard]] static RasterCacheEpoch::Token publish_inner(Tree& tree, NodeId id) {
        auto& epoch = tree.raster_cache_epochs_.at(id)->inner->epoch;
        const auto token = epoch.capture();
        NUI_CHECK(epoch.commit(token));
        return token;
    }

    [[nodiscard]] static bool inner_reusable(
        const Tree& tree, NodeId id, const RasterCacheEpoch::Token& token) {
        return tree.raster_cache_epochs_.at(id)->inner->epoch.reusable(token);
    }
};

} // namespace ui::detail

namespace {

using CacheAccess = ui::detail::RasterCacheAccess;
constexpr ui::Size kViewport{96.0f, 32.0f};

struct Observation {
    ui::NodeId id{ui::kInvalidNodeId};
    ui::Size minimum{12.0f, 12.0f};
    ui::Size preferred{32.0f, 24.0f};
    std::optional<float> baseline;
    ui::Rect bounds{};
    ui::Color color{1.0f, 0.0f, 0.0f, 1.0f};
    std::function<void()> invalidate;
    std::function<void(ui::PaintContext&)> on_paint;
    int mounts{};
    int unmounts{};
    int paints{};
    int pointer_down{};
    int pointer_up{};
    int key_down{};
    int focus_in{};
    int focus_out{};
    bool interactive{};
};

class ProbeComponent final : public ui::Component {
public:
    explicit ProbeComponent(std::shared_ptr<Observation> observation)
        : observation_(std::move(observation)) {}

    [[nodiscard]] ui::Size measure(
        const std::vector<ui::ChildMetrics>&) const override {
        return observation_->preferred;
    }

    [[nodiscard]] ui::Size minimum_size(
        const std::vector<ui::ChildMetrics>&) const override {
        return observation_->minimum;
    }

    [[nodiscard]] std::optional<float> first_baseline(ui::Size) const override {
        return observation_->baseline;
    }

    [[nodiscard]] bool focusable() const noexcept override {
        return observation_->interactive;
    }

    [[nodiscard]] ui::SemanticInfo semantics() const override {
        ui::SemanticInfo result;
        result.role = ui::SemanticRole::Button;
        result.name = "Cached layer probe";
        result.focusable = observation_->interactive;
        result.actions = {ui::SemanticAction::Activate, ui::SemanticAction::Focus};
        return result;
    }

    void mount(ui::MountContext& context) override {
        observation_->id = context.node_id();
        observation_->invalidate = context.invalidator();
        ++observation_->mounts;
    }

    void unmount(ui::LifecycleContext&) override { ++observation_->unmounts; }

    void focus_changed(bool focused, ui::FocusContext& context) override {
        if (focused) ++observation_->focus_in;
        else ++observation_->focus_out;
        observation_->bounds = context.bounds();
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext& context) override {
        if (event.type == ui::InputType::PointerDown) {
            ++observation_->pointer_down;
            context.capture_pointer();
            return ui::EventResult::Handled;
        }
        if (event.type == ui::InputType::PointerUp) {
            ++observation_->pointer_up;
            return ui::EventResult::Handled;
        }
        if (event.type == ui::InputType::KeyDown && event.key == ui::Key::Space) {
            ++observation_->key_down;
            return ui::EventResult::Handled;
        }
        return ui::EventResult::Ignored;
    }

    void paint(ui::PaintContext& context) const override {
        ++observation_->paints;
        observation_->bounds = context.bounds();
        if (observation_->on_paint) observation_->on_paint(context);
        context.painter().fill_rounded_rect(context.bounds(), 0.0f, observation_->color);
    }

private:
    std::shared_ptr<Observation> observation_;
};

class Probe {
public:
    explicit Probe(std::shared_ptr<Observation> observation)
        : observation_(std::move(observation)) {}

    ui::Spec spec() && {
        auto observation = std::move(observation_);
        return {[observation = std::move(observation)] {
            return std::make_unique<ProbeComponent>(observation);
        }, {}};
    }

private:
    std::shared_ptr<Observation> observation_;
};

void check_rect(ui::Rect actual, ui::Rect expected) {
    NUI_CHECK_NEAR(actual.x, expected.x, 0.001f);
    NUI_CHECK_NEAR(actual.y, expected.y, 0.001f);
    NUI_CHECK_NEAR(actual.w, expected.w, 0.001f);
    NUI_CHECK_NEAR(actual.h, expected.h, 0.001f);
}

void check_metrics(const ui::ChildMetrics& actual, const ui::ChildMetrics& expected) {
    NUI_CHECK_NEAR(actual.minimum.w, expected.minimum.w, 0.001f);
    NUI_CHECK_NEAR(actual.minimum.h, expected.minimum.h, 0.001f);
    NUI_CHECK_NEAR(actual.preferred.w, expected.preferred.w, 0.001f);
    NUI_CHECK_NEAR(actual.preferred.h, expected.preferred.h, 0.001f);
    NUI_CHECK_NEAR(actual.flex.grow, expected.flex.grow, 0.001f);
    NUI_CHECK_NEAR(actual.flex.shrink, expected.flex.shrink, 0.001f);
    NUI_CHECK(actual.first_baseline == expected.first_baseline);
    NUI_CHECK(actual.participates_in_layout == expected.participates_in_layout);
}

void settle(ui::Tree& tree) {
    test::MockPlatform platform;
    SkCanvas canvas;
    tree.layout(kViewport);
    tree.paint(canvas, platform);
}

CacheAccess::Token publish(ui::Tree& tree, ui::NodeId id) {
    auto token = CacheAccess::capture(tree, id);
    NUI_CHECK(!token.expired());
    NUI_CHECK(CacheAccess::commit(tree, id, token));
    return token;
}

void zero_dependencies_reuse_after_scene_repaint() {
    auto cached = std::make_shared<Observation>();
    int surrounding_paints{};
    ui::UI scene{ui::Stack{
        ui::Canvas{kViewport, [&](ui::CanvasContext2D&) { ++surrounding_paints; }},
        ui::CachedLayer{Probe{cached}}}};
    ui::HeadlessRenderer renderer{kViewport};
    NUI_CHECK(renderer.render(scene));
    const auto cold_pixels = renderer.rgba_pixels();
    NUI_CHECK(cached->paints == 1);
    NUI_CHECK(surrounding_paints == 1);

    scene.invalidate();
    NUI_CHECK(renderer.render(scene));
    NUI_CHECK(cached->paints == 1);
    NUI_CHECK(surrounding_paints == 2);
    NUI_CHECK(renderer.rgba_pixels() == cold_pixels);

    cached->color = {0.0f, 1.0f, 0.0f, 1.0f};
    cached->invalidate();
    NUI_CHECK(scene.paint_dirty());
    NUI_CHECK(renderer.render(scene));
    NUI_CHECK(cached->paints == 2);
    NUI_CHECK(renderer.rgba_pixels() != cold_pixels);
    scene.invalidate();
    NUI_CHECK(renderer.render(scene));
    NUI_CHECK(cached->paints == 2);
}

struct ThemeValue {
    int accent{};
    bool operator==(const ThemeValue&) const = default;
};

void heterogeneous_dependencies_are_semantic_and_coalesced() {
    ui::State<ThemeValue> theme{ThemeValue{0}};
    ui::State<int> revision{0};
    auto cached = std::make_shared<Observation>();
    cached->on_paint = [&](ui::PaintContext&) {
        cached->color = theme.get().accent == 0
            ? ui::Color{1.0f, 0.0f, 0.0f, 1.0f}
            : ui::Color{0.0f, 0.0f, 1.0f, 1.0f};
    };
    ui::UI scene{ui::CachedLayer{Probe{cached}}.depends(theme, revision)};
    ui::HeadlessRenderer renderer{kViewport};
    NUI_CHECK(renderer.render(scene));
    const auto original = renderer.rgba_pixels();
    int redraws{};
    scene.set_invalidation_callback([&] { ++redraws; });

    theme.set(ThemeValue{0});
    revision.set(0);
    NUI_CHECK(redraws == 0);
    NUI_CHECK(!scene.dirty());
    NUI_CHECK(renderer.render(scene));
    NUI_CHECK(cached->paints == 1);

    theme.set(ThemeValue{1});
    revision.set(1);
    NUI_CHECK(redraws == 1);
    NUI_CHECK(scene.paint_dirty());
    NUI_CHECK(!scene.layout_dirty());
    NUI_CHECK(cached->paints == 1);
    NUI_CHECK(renderer.render(scene));
    NUI_CHECK(cached->paints == 2);
    NUI_CHECK(renderer.rgba_pixels() != original);
    NUI_CHECK(renderer.render(scene));
    NUI_CHECK(cached->paints == 2);
    cached->on_paint = {};
}

void duplicate_dependencies_have_one_logical_notification() {
    ui::State<int> revision{0};
    auto cached = std::make_shared<Observation>();
    ui::Tree tree{ui::compile(ui::make_spec(
        ui::CachedLayer{Probe{cached}}.depends(revision, revision, revision)))};
    tree.mount();
    settle(tree);
    NUI_CHECK(ui::TreeTestAccess::raster_records(tree) == 1);
    NUI_CHECK(ui::detail::CachedLayerTestAccess::subscriptions(tree, cached->id) == 1);
    const auto before = publish(tree, cached->id);
    CacheAccess::Token during_redraw;
    tree.set_invalidation_callback([&] {
        // Capture without painting makes a second duplicate callback observable:
        // it would revoke this generation even though damage is already queued.
        during_redraw = CacheAccess::capture(tree, cached->id);
    });
    revision.set(1);
    const auto after = CacheAccess::capture(tree, cached->id);
    NUI_CHECK(after.generation() == before.generation() + 1);
    NUI_CHECK(during_redraw.same_content(after));
    NUI_CHECK(!CacheAccess::reusable(tree, cached->id, before));
}

void shared_and_private_dependencies_invalidate_only_their_layers() {
    ui::State<int> shared{0};
    ui::State<ThemeValue> private_theme{ThemeValue{0}};
    auto first = std::make_shared<Observation>();
    auto second = std::make_shared<Observation>();
    auto static_layer = std::make_shared<Observation>();
    ui::UI scene{ui::Row{
        ui::CachedLayer{Probe{first}}.depends(shared, private_theme),
        ui::CachedLayer{Probe{second}}.depends(shared),
        ui::CachedLayer{Probe{static_layer}}}.gap(0.0f)};
    ui::HeadlessRenderer renderer{kViewport};
    NUI_CHECK(renderer.render(scene));
    private_theme.set(ThemeValue{1});
    NUI_CHECK(renderer.render(scene));
    NUI_CHECK(first->paints == 2);
    NUI_CHECK(second->paints == 1);
    NUI_CHECK(static_layer->paints == 1);
    shared.set(1);
    NUI_CHECK(renderer.render(scene));
    NUI_CHECK(first->paints == 3);
    NUI_CHECK(second->paints == 2);
    NUI_CHECK(static_layer->paints == 1);
}

void dependency_order_and_copied_recipes_preserve_instance_isolation() {
    ui::State<int> revision{0};
    ui::State<ThemeValue> theme{ThemeValue{0}};
    auto first = std::make_shared<Observation>();
    auto second = std::make_shared<Observation>();
    ui::UI a{ui::CachedLayer{Probe{first}}.depends(theme, revision)};
    ui::UI b{ui::CachedLayer{Probe{second}}.depends(revision, theme)};
    ui::HeadlessRenderer renderer{kViewport};
    NUI_CHECK(renderer.render(a));
    NUI_CHECK(renderer.render(b));
    NUI_CHECK(renderer.render(a));
    NUI_CHECK(first->paints == 1 && second->paints == 1);
    revision.set(1);
    NUI_CHECK(renderer.render(a));
    NUI_CHECK(renderer.render(b));
    NUI_CHECK(first->paints == 2 && second->paints == 2);

    auto copied = std::make_shared<Observation>();
    auto recipe = ui::CachedLayer{Probe{copied}}.depends(revision).spec();
    auto temporary = std::make_unique<ui::UI>(recipe);
    ui::UI survivor{recipe};
    NUI_CHECK(renderer.render(*temporary));
    NUI_CHECK(renderer.render(survivor));
    NUI_CHECK(copied->paints == 2);
    temporary.reset();
    revision.set(2);
    NUI_CHECK(renderer.render(survivor));
    NUI_CHECK(copied->paints == 3);
    NUI_CHECK(renderer.render(survivor));
    NUI_CHECK(copied->paints == 3);
}

void natural_nested_layers_keep_independent_dependency_declarations() {
    ui::State<int> outer_revision{0};
    ui::State<int> inner_revision{0};
    auto cached = std::make_shared<Observation>();
    auto recipe = ui::CachedLayer{
        ui::CachedLayer{Probe{cached}}.depends(inner_revision)}.depends(outer_revision).spec();
    NUI_CHECK(recipe.cached_layer && recipe.cached_layer->inner);
    NUI_CHECK(!recipe.cached_layer->inner->inner);
    ui::Tree tree{ui::compile(std::move(recipe))};
    tree.mount();
    settle(tree);
    NUI_CHECK(ui::TreeTestAccess::raster_records(tree) == 1);
    NUI_CHECK(ui::detail::CachedLayerTestAccess::layers(tree, cached->id) == 2);
    const auto outer = publish(tree, cached->id);
    const auto inner = ui::detail::CachedLayerTestAccess::publish_inner(tree, cached->id);
    outer_revision.set(1);
    NUI_CHECK(!CacheAccess::reusable(tree, cached->id, outer));
    NUI_CHECK(ui::detail::CachedLayerTestAccess::inner_reusable(tree, cached->id, inner));
    const auto next_outer = publish(tree, cached->id);
    inner_revision.set(1);
    NUI_CHECK(!CacheAccess::reusable(tree, cached->id, next_outer));
    NUI_CHECK(!ui::detail::CachedLayerTestAccess::inner_reusable(tree, cached->id, inner));
}

void omitted_dependency_does_not_hide_descendant_invalidation() {
    ui::State<int> declared{0};
    ui::State<bool> enabled{true};
    auto cached = std::make_shared<Observation>();
    ui::UI scene{ui::CachedLayer{
        ui::Enabled{enabled, Probe{cached}}}.depends(declared)};
    ui::HeadlessRenderer renderer{kViewport};
    NUI_CHECK(renderer.render(scene));
    const auto initial = renderer.rgba_pixels();
    cached->color = {0.0f, 1.0f, 0.0f, 1.0f};
    cached->invalidate();
    NUI_CHECK(renderer.render(scene));
    NUI_CHECK(cached->paints == 2);
    NUI_CHECK(renderer.rgba_pixels() != initial);
    enabled.set(false);
    NUI_CHECK(renderer.render(scene));
    NUI_CHECK(cached->paints == 3);
    NUI_CHECK(!scene.component_availability(cached->id)->enabled);
}

void source_destruction_before_and_after_mount_is_safe() {
    ui::HeadlessRenderer renderer{kViewport};
    for (const bool destroy_before_mount : {false, true}) {
        auto source = std::make_unique<ui::State<int>>(0);
        auto binding = source->binding();
        auto cached = std::make_shared<Observation>();
        auto recipe = ui::CachedLayer{Probe{cached}}.depends(*source).spec();
        if (destroy_before_mount) source.reset();
        ui::UI scene{std::move(recipe)};
        NUI_CHECK(renderer.render(scene));
        source.reset();
        NUI_CHECK(!binding.valid());
        binding.set(1);
        NUI_CHECK(!scene.dirty());
        scene.invalidate();
        NUI_CHECK(renderer.render(scene));
        NUI_CHECK(cached->paints == 1);
    }
}

void unmount_remount_retires_subscriptions_and_cache_identity() {
    ui::State<int> revision{0};
    auto cached = std::make_shared<Observation>();
    ui::Tree tree{ui::compile(ui::make_spec(
        ui::CachedLayer{Probe{cached}}.depends(revision)))};
    tree.mount();
    settle(tree);
    const auto previous = publish(tree, cached->id);
    const auto stale = CacheAccess::invalidator(tree, cached->id);
    tree.unmount();
    NUI_CHECK(previous.expired());
    NUI_CHECK(ui::TreeTestAccess::raster_records(tree) == 0);
    revision.set(1);
    tree.mount();
    settle(tree);
    const auto current = publish(tree, cached->id);
    NUI_CHECK(!current.same_lifetime(previous));
    stale();
    NUI_CHECK(CacheAccess::reusable(tree, cached->id, current));
    revision.set(2);
    NUI_CHECK(!CacheAccess::reusable(tree, cached->id, current));
    NUI_CHECK(cached->mounts == 2 && cached->unmounts == 1);
}

void keyed_remove_reinsert_and_removal_inside_dependency_callback() {
    ui::State<int> revision{0};
    ui::State<std::vector<int>> keys{{1}};
    auto cached = std::make_shared<Observation>();
    ui::Tree tree{ui::compile(ui::make_spec(ui::ForEach{
        keys, [](int value) { return value; }, [&](int) {
            return ui::CachedLayer{Probe{cached}}.depends(revision);
        }}))};
    tree.mount();
    settle(tree);
    const auto previous_id = cached->id;
    const auto previous = publish(tree, cached->id);
    const auto stale = CacheAccess::invalidator(tree, cached->id);
    bool remove_on_redraw{};
    tree.set_invalidation_callback([&] {
        if (!std::exchange(remove_on_redraw, false)) return;
        keys.set({});
        tree.layout(kViewport);
    });
    remove_on_redraw = true;
    revision.set(1);
    NUI_CHECK(previous.expired());
    NUI_CHECK(cached->unmounts == 1);
    NUI_CHECK(ui::TreeTestAccess::raster_records(tree) == 0);
    settle(tree);
    keys.set({1});
    settle(tree);
    NUI_CHECK(cached->id != previous_id);
    const auto current = publish(tree, cached->id);
    stale();
    NUI_CHECK(CacheAccess::reusable(tree, cached->id, current));
    revision.set(2);
    NUI_CHECK(!CacheAccess::reusable(tree, cached->id, current));
    NUI_CHECK(ui::TreeTestAccess::raster_records(tree) == 1);
}

void earlier_observer_can_remove_a_layer_before_its_notification() {
    ui::State<int> revision{0};
    ui::State<bool> visible{true};
    std::function<void()> remove;
    const auto observer = revision.observe([&](const int&) { if (remove) remove(); });
    auto cached = std::make_shared<Observation>();
    ui::Tree tree{ui::compile(ui::make_spec(
        ui::If{visible, ui::CachedLayer{Probe{cached}}.depends(revision)}))};
    tree.mount();
    settle(tree);
    const auto previous = publish(tree, cached->id);
    remove = [&] { visible.set(false); tree.layout(kViewport); };
    revision.set(1);
    NUI_CHECK(previous.expired());
    NUI_CHECK(ui::TreeTestAccess::raster_records(tree) == 0);
    remove = {};
    visible.set(true);
    settle(tree);
    const auto current = publish(tree, cached->id);
    revision.set(2);
    NUI_CHECK(!CacheAccess::reusable(tree, cached->id, current));
    NUI_CHECK(observer.active());
}

void recursive_changes_and_throwing_redraw_recover() {
    ui::State<int> first{0};
    ui::State<int> second{0};
    const auto recursive = first.observe([&](const int& value) {
        if (value == 1) { second.set(1); first.set(2); }
    });
    auto cached = std::make_shared<Observation>();
    ui::UI scene{ui::CachedLayer{Probe{cached}}.depends(first, second)};
    ui::HeadlessRenderer renderer{kViewport};
    NUI_CHECK(renderer.render(scene));
    first.set(1);
    NUI_CHECK(first.get() == 2 && second.get() == 1);
    NUI_CHECK(renderer.render(scene));
    NUI_CHECK(cached->paints == 2);

    scene.set_invalidation_callback([] {
        throw std::runtime_error("injected dependency redraw failure");
    });
    bool caught{};
    try { first.set(3); } catch (const std::runtime_error&) { caught = true; }
    NUI_CHECK(caught);
    NUI_CHECK(scene.paint_dirty());
    scene.clear_invalidation_callback();
    NUI_CHECK(renderer.render(scene));
    NUI_CHECK(cached->paints == 3);
    second.set(2);
    NUI_CHECK(renderer.render(scene));
    NUI_CHECK(cached->paints == 4);
    NUI_CHECK(recursive.active());
}

void earlier_throwing_observer_cannot_reuse_stale_dependency_pixels() {
    ui::State<int> revision{0};
    bool fail{};
    const auto observer = revision.observe([&](const int&) {
        if (fail) throw std::runtime_error("injected earlier observer failure");
    });
    auto cached = std::make_shared<Observation>();
    cached->on_paint = [&](ui::PaintContext&) {
        cached->color = revision.get() == 0
            ? ui::Color{1.0f, 0.0f, 0.0f, 1.0f}
            : ui::Color{0.0f, 1.0f, 0.0f, 1.0f};
    };
    ui::UI scene{ui::CachedLayer{Probe{cached}}.depends(revision)};
    ui::HeadlessRenderer renderer{kViewport};
    NUI_CHECK(renderer.render(scene));
    const auto original = renderer.rgba_pixels();
    fail = true;
    bool caught{};
    try { revision.set(1); } catch (const std::runtime_error&) { caught = true; }
    NUI_CHECK(caught);
    NUI_CHECK(revision.get() == 1);
    scene.invalidate();
    NUI_CHECK(renderer.render(scene));
    NUI_CHECK(cached->paints == 2);
    NUI_CHECK(renderer.rgba_pixels() != original);
    fail = false;
    revision.set(2);
    NUI_CHECK(renderer.render(scene));
    NUI_CHECK(cached->paints == 3);
    NUI_CHECK(observer.active());
    cached->on_paint = {};
}

void every_subscription_setup_failure_rolls_back_and_allows_retry() {
    ui::State<int> revision{0};
    ui::State<ThemeValue> theme{ThemeValue{0}};
    auto survivor_log = std::make_shared<Observation>();
    ui::Tree survivor{ui::compile(ui::make_spec(
        ui::CachedLayer{Probe{survivor_log}}.depends(revision, theme)))};
    survivor.mount();
    settle(survivor);
    const auto checkpoint_count =
        ui::detail::CachedLayerTestAccess::setup_checkpoints(survivor);
    NUI_CHECK(checkpoint_count >= 5);

    for (std::size_t checkpoint = 0; checkpoint < checkpoint_count; ++checkpoint) {
        auto cached = std::make_shared<Observation>();
        ui::Tree tree{ui::compile(ui::make_spec(
            ui::CachedLayer{Probe{cached}}.depends(revision, theme)))};
        ui::detail::CachedLayerTestAccess::fail_setup_after(tree, checkpoint);
        bool caught{};
        try { tree.mount(); } catch (const std::bad_alloc&) { caught = true; }
        NUI_CHECK(caught);
        NUI_CHECK(!ui::detail::CachedLayerTestAccess::setup_fault_armed(tree));
        NUI_CHECK(ui::TreeTestAccess::raster_records(tree) == 0);

        const auto survivor_before = publish(survivor, survivor_log->id);
        revision.set(revision.get() + 1);
        NUI_CHECK(!CacheAccess::reusable(survivor, survivor_log->id, survivor_before));
        NUI_CHECK(ui::TreeTestAccess::raster_records(tree) == 0);
        settle(survivor);

        tree.mount();
        settle(tree);
        NUI_CHECK(ui::detail::CachedLayerTestAccess::subscriptions(tree, cached->id) == 2);
        const auto retry = publish(tree, cached->id);
        theme.set(ThemeValue{theme.get().accent + 1});
        NUI_CHECK(!CacheAccess::reusable(tree, cached->id, retry));
        settle(tree);
        NUI_CHECK(!publish(tree, cached->id).expired());
        settle(survivor);
    }
}

void headless_failed_paint_recovers_canvas_and_cache() {
    auto cached = std::make_shared<Observation>();
    bool fail = true;
    cached->on_paint = [&](ui::PaintContext& context) {
        if (!std::exchange(fail, false)) return;
        context.painter().translate(7.0f, 5.0f);
        throw std::runtime_error("injected cached paint failure");
    };
    ui::UI scene{ui::CachedLayer{Probe{cached}}};
    ui::HeadlessRenderer renderer{kViewport, 2.0f};
    bool caught{};
    try { (void)renderer.render(scene); }
    catch (const std::runtime_error&) { caught = true; }
    NUI_CHECK(caught);
    NUI_CHECK(scene.paint_dirty());
    NUI_CHECK(renderer.render(scene));
    NUI_CHECK(cached->paints == 2);
    const auto recovered = renderer.rgba_pixels();
    scene.invalidate();
    NUI_CHECK(renderer.render(scene));
    NUI_CHECK(cached->paints == 2);
    NUI_CHECK(renderer.rgba_pixels() == recovered);

    auto reference = std::make_shared<Observation>();
    ui::UI uncached{Probe{reference}};
    ui::HeadlessRenderer reference_renderer{kViewport, 2.0f};
    NUI_CHECK(reference_renderer.render(uncached));
    NUI_CHECK(reference_renderer.rgba_pixels() == recovered);
}

void headless_render_and_resize_reentry_fail_before_mutation_and_recover() {
    for (const bool resize_reentry : {false, true}) {
        ui::HeadlessRenderer renderer{kViewport, 2.0f};
        auto other = std::make_shared<Observation>();
        ui::UI other_scene{ui::CachedLayer{Probe{other}}};
        auto cached = std::make_shared<Observation>();
        bool attempt = true;
        cached->on_paint = [&](ui::PaintContext&) {
            if (!std::exchange(attempt, false)) return;
            if (resize_reentry) renderer.resize({12.0f, 12.0f}, 1.0f);
            else (void)renderer.render(other_scene);
        };
        ui::UI scene{ui::CachedLayer{Probe{cached}}};
        bool caught{};
        try { (void)renderer.render(scene); }
        catch (const std::logic_error&) { caught = true; }
        NUI_CHECK(caught);
        NUI_CHECK(scene.paint_dirty());
        NUI_CHECK(other->paints == 0);
        NUI_CHECK(renderer.logical_size().w == kViewport.w);
        NUI_CHECK(renderer.logical_size().h == kViewport.h);
        NUI_CHECK(renderer.scale_factor() == 2.0f);
        NUI_CHECK(renderer.pixel_width() == 192 && renderer.pixel_height() == 64);
        NUI_CHECK(renderer.render(scene));
        NUI_CHECK(cached->paints == 2);
        scene.invalidate();
        NUI_CHECK(renderer.render(scene));
        NUI_CHECK(cached->paints == 2);
        NUI_CHECK(renderer.render(other_scene));
        NUI_CHECK(other->paints == 1);
        NUI_CHECK(renderer.render(scene));
        NUI_CHECK(cached->paints == 2);
    }
}

void layout_flex_baseline_and_visibility_match_the_child() {
    auto plain_a = std::make_shared<Observation>();
    auto plain_b = std::make_shared<Observation>();
    auto cached_a = std::make_shared<Observation>();
    auto cached_b = std::make_shared<Observation>();
    plain_a->baseline = 18.0f;
    cached_a->baseline = 18.0f;
    ui::UI plain{ui::Row{
        ui::Flex{Probe{plain_a}}.grow(1.0f).shrink(1.0f),
        ui::Flex{Probe{plain_b}}.grow(2.0f).shrink(1.0f)}.gap(10.0f)};
    ui::UI cached{ui::Row{
        ui::CachedLayer{ui::Flex{Probe{cached_a}}.grow(1.0f).shrink(1.0f)},
        ui::CachedLayer{ui::Flex{Probe{cached_b}}.grow(2.0f).shrink(1.0f)}}.gap(10.0f)};
    check_metrics(cached.measure(), plain.measure());
    ui::HeadlessRenderer plain_renderer{{240.0f, 40.0f}};
    ui::HeadlessRenderer cached_renderer{{240.0f, 40.0f}};
    for (const float width : {240.0f, 50.0f}) {
        plain_renderer.resize({width, 40.0f});
        cached_renderer.resize({width, 40.0f});
        NUI_CHECK(plain_renderer.render(plain));
        NUI_CHECK(cached_renderer.render(cached));
        check_rect(cached_a->bounds, plain_a->bounds);
        check_rect(cached_b->bounds, plain_b->bounds);
        NUI_CHECK(cached_renderer.rgba_pixels() == plain_renderer.rgba_pixels());
    }

    ui::UI baseline_plain{Probe{plain_a}};
    ui::UI baseline_cached{ui::CachedLayer{Probe{cached_a}}};
    check_metrics(baseline_cached.measure(), baseline_plain.measure());
    NUI_CHECK(baseline_cached.measure().first_baseline == std::optional<float>{18.0f});

    ui::State<ui::VisibilityMode> visibility{ui::VisibilityMode::Visible};
    ui::UI visible_plain{ui::Row{
        ui::Visibility{visibility, Probe{plain_a}}, Probe{plain_b}}.gap(9.0f)};
    ui::UI visible_cached{ui::Row{
        ui::CachedLayer{ui::Visibility{visibility, Probe{cached_a}}},
        Probe{cached_b}}.gap(9.0f)};
    plain_renderer.resize(kViewport);
    cached_renderer.resize(kViewport);
    for (const auto mode : {ui::VisibilityMode::Visible, ui::VisibilityMode::Hidden,
                            ui::VisibilityMode::Collapsed, ui::VisibilityMode::Visible}) {
        visibility.set(mode);
        check_metrics(visible_cached.measure(), visible_plain.measure());
        NUI_CHECK(plain_renderer.render(visible_plain));
        NUI_CHECK(cached_renderer.render(visible_cached));
        check_rect(cached_b->bounds, plain_b->bounds);
        NUI_CHECK(cached_renderer.rgba_pixels() == plain_renderer.rgba_pixels());
    }
}

void input_focus_and_semantics_match_the_child() {
    auto plain_a = std::make_shared<Observation>();
    auto plain_b = std::make_shared<Observation>();
    auto cached_a = std::make_shared<Observation>();
    auto cached_b = std::make_shared<Observation>();
    for (const auto& observation : {plain_a, plain_b, cached_a, cached_b}) {
        observation->interactive = true;
    }
    ui::UI plain{ui::Row{Probe{plain_a}, Probe{plain_b}}.gap(0.0f)};
    ui::UI cached{ui::Row{
        ui::CachedLayer{Probe{cached_a}}, ui::CachedLayer{Probe{cached_b}}}.gap(0.0f)};
    test::MockPlatform plain_platform;
    test::MockPlatform cached_platform;
    plain.resize(kViewport);
    cached.resize(kViewport);
    plain.activate(plain_platform);
    cached.activate(cached_platform);
    NUI_CHECK(plain_a->id == cached_a->id && plain_b->id == cached_b->id);
    NUI_CHECK(plain.component_semantics(plain_a->id) == cached.component_semantics(cached_a->id));
    for (const auto& event : {
            test::pointer(ui::InputType::PointerDown, 8.0f, 8.0f),
            test::pointer(ui::InputType::PointerUp, 8.0f, 8.0f),
            test::key(ui::Key::Tab), test::key(ui::Key::Space)}) {
        NUI_CHECK(plain.dispatch(event, plain_platform) == cached.dispatch(event, cached_platform));
    }
    NUI_CHECK(plain_a->pointer_down == 1 && cached_a->pointer_down == 1);
    NUI_CHECK(plain_a->pointer_up == cached_a->pointer_up);
    NUI_CHECK(plain_a->focus_in == cached_a->focus_in);
    NUI_CHECK(plain_a->focus_out == cached_a->focus_out);
    NUI_CHECK(plain_b->focus_in == cached_b->focus_in);
    NUI_CHECK(plain_b->key_down == 1 && cached_b->key_down == 1);
    NUI_CHECK(plain_platform.pointer_capture_begin_count == cached_platform.pointer_capture_begin_count);
    NUI_CHECK(plain_platform.pointer_capture_end_count == cached_platform.pointer_capture_end_count);
    NUI_CHECK(plain.component_semantics(plain_b->id) == cached.component_semantics(cached_b->id));
    check_rect(cached_b->bounds, plain_b->bounds);
}

void suite() {
    zero_dependencies_reuse_after_scene_repaint();
    heterogeneous_dependencies_are_semantic_and_coalesced();
    duplicate_dependencies_have_one_logical_notification();
    shared_and_private_dependencies_invalidate_only_their_layers();
    dependency_order_and_copied_recipes_preserve_instance_isolation();
    natural_nested_layers_keep_independent_dependency_declarations();
    omitted_dependency_does_not_hide_descendant_invalidation();
    source_destruction_before_and_after_mount_is_safe();
    unmount_remount_retires_subscriptions_and_cache_identity();
    keyed_remove_reinsert_and_removal_inside_dependency_callback();
    earlier_observer_can_remove_a_layer_before_its_notification();
    recursive_changes_and_throwing_redraw_recover();
    earlier_throwing_observer_cannot_reuse_stale_dependency_pixels();
    every_subscription_setup_failure_rolls_back_and_allows_retry();
    headless_failed_paint_recovers_canvas_and_cache();
    headless_render_and_resize_reentry_fail_before_mutation_and_recover();
    layout_flex_baseline_and_visibility_match_the_child();
    input_focus_and_semantics_match_the_child();
}

} // namespace

int main() { return test::run("cached_layer", &suite); }
