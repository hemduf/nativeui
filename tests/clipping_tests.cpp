#include "test_support.hpp"

#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ui::detail {

struct DynamicReconcileFaultAccess {
    static Node* first_dynamic(Tree& tree, Node& node) noexcept {
        if (tree.dynamic_source(node)) return &node;
        for (auto& child : node.children) {
            if (auto* found = first_dynamic(tree, *child)) return found;
        }
        return nullptr;
    }

    static bool reconcile_first(Tree& tree) {
        if (!tree.root_) return false;
        auto* owner = first_dynamic(tree, *tree.root_);
        return owner ? tree.reconcile_dynamic_node(*owner) : false;
    }

    [[nodiscard]] static std::size_t focusable_count(const Tree& tree) noexcept {
        return tree.focusables_.size();
    }
};

} // namespace ui::detail

namespace {

struct HitState {
    int pointer_down{};
    std::vector<ui::Rect> focus_bounds;
};

class HitComponent final : public ui::Component {
public:
    explicit HitComponent(std::shared_ptr<HitState> state) : state_(std::move(state)) {}
    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {100.0f, 40.0f};
    }
    void focus_changed(bool, ui::FocusContext& context) override {
        state_->focus_bounds.push_back(context.bounds());
    }
    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type == ui::InputType::PointerDown) {
            ++state_->pointer_down;
            return ui::EventResult::Handled;
        }
        return ui::EventResult::Ignored;
    }
    void paint(ui::PaintContext&) const override {}
private:
    std::shared_ptr<HitState> state_;
};

class Hit {
public:
    explicit Hit(std::shared_ptr<HitState> state) : state_(std::move(state)) {}
    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{[state = std::move(state)] {
            return std::make_unique<HitComponent>(state);
        }, {}};
    }
private:
    std::shared_ptr<HitState> state_;
};

struct ThrowingPaintState {
    bool throw_on_paint{true};
    int paints{};
};

class ThrowingPaintComponent final : public ui::Component {
public:
    explicit ThrowingPaintComponent(std::shared_ptr<ThrowingPaintState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {40.0f, 40.0f};
    }

    void paint(ui::PaintContext&) const override {
        ++state_->paints;
        if (state_->throw_on_paint) {
            throw std::runtime_error("T130 throwing paint probe");
        }
    }

private:
    std::shared_ptr<ThrowingPaintState> state_;
};

class ThrowingPaint {
public:
    explicit ThrowingPaint(std::shared_ptr<ThrowingPaintState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<ThrowingPaintComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<ThrowingPaintState> state_;
};

struct FocusRollbackState {
    bool focusable{};
    bool throw_mount{};
    bool throw_activate{};
    std::function<void()> on_deactivate;
    std::function<void()> on_unmount;
    int mounts{};
    int activates{};
    int deactivates{};
    int unmounts{};
    int destroyed{};
};

class FocusRollbackComponent final : public ui::Component {
public:
    FocusRollbackComponent(std::string name, std::shared_ptr<FocusRollbackState> state)
        : name_(std::move(name)), state_(std::move(state)) {}

    ~FocusRollbackComponent() override { ++state_->destroyed; }

    [[nodiscard]] bool focusable() const noexcept override { return state_->focusable; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {40.0f, 20.0f};
    }

    void mount(ui::MountContext&) override {
        ++state_->mounts;
        if (state_->throw_mount) {
            state_->throw_mount = false;
            throw std::runtime_error(name_ + ".mount");
        }
    }

    void activate(ui::LifecycleContext&) override {
        ++state_->activates;
        if (state_->throw_activate) {
            state_->throw_activate = false;
            throw std::runtime_error(name_ + ".activate");
        }
    }

    void deactivate(ui::LifecycleContext&) override {
        ++state_->deactivates;
        if (state_->on_deactivate) state_->on_deactivate();
    }

    void unmount(ui::LifecycleContext&) override {
        ++state_->unmounts;
        if (state_->on_unmount) state_->on_unmount();
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::string name_;
    std::shared_ptr<FocusRollbackState> state_;
};

class FocusRollbackProbe {
public:
    FocusRollbackProbe(std::string name, std::shared_ptr<FocusRollbackState> state)
        : name_(std::move(name)), state_(std::move(state)) {}

    ui::Spec spec() && {
        auto name = std::move(name_);
        auto state = std::move(state_);
        return ui::Spec{
            [name = std::move(name), state = std::move(state)]() mutable {
                return std::make_unique<FocusRollbackComponent>(
                    std::move(name), std::move(state));
            },
            {}};
    }

private:
    std::string name_;
    std::shared_ptr<FocusRollbackState> state_;
};

bool red(ui::Rgba8 pixel) {
    return pixel.r > 220 && pixel.g < 40 && pixel.b < 40 && pixel.a > 220;
}

bool green(ui::Rgba8 pixel) {
    return pixel.g > 220 && pixel.r < 40 && pixel.b < 40 && pixel.a > 220;
}

void dynamic_focus_registry_rollback_contract(bool activation_failure) {
    using Access = ui::detail::DynamicReconcileFaultAccess;

    ui::State<bool> visible{false};
    auto retained = std::make_shared<FocusRollbackState>();
    auto inserted_focus = std::make_shared<FocusRollbackState>();
    auto later = std::make_shared<FocusRollbackState>();
    retained->focusable = true;
    inserted_focus->focusable = true;
    if (activation_failure) {
        later->throw_activate = true;
    } else {
        later->throw_mount = true;
    }

    test::MockPlatform platform;
    ui::Tree tree{ui::compile(
        ui::Column{
            FocusRollbackProbe{"retained", retained},
            ui::If{
                visible,
                ui::Column{
                    FocusRollbackProbe{"inserted-focus", inserted_focus},
                    FocusRollbackProbe{"later", later}}}}
            .spec())};
    tree.mount();
    tree.layout({160.0f, 80.0f});
    tree.activate_focus(platform);
    NUI_CHECK(Access::focusable_count(tree) == 1);

    std::size_t rollback_observations = 0;
    bool rollback_saw_only_retained_focus = true;
    auto observe_rollback_focus = [&] {
        ++rollback_observations;
        rollback_saw_only_retained_focus =
            rollback_saw_only_retained_focus && Access::focusable_count(tree) == 1;
    };
    if (activation_failure) {
        inserted_focus->on_deactivate = observe_rollback_focus;
    } else {
        inserted_focus->on_unmount = observe_rollback_focus;
    }

    visible.set(true);
    std::string propagated;
    try {
        (void)Access::reconcile_first(tree);
    } catch (const std::runtime_error& error) {
        propagated = error.what();
    }
    NUI_CHECK(propagated == (activation_failure ? "later.activate" : "later.mount"));
    NUI_CHECK(rollback_observations == 1);
    NUI_CHECK(rollback_saw_only_retained_focus);
    NUI_CHECK(inserted_focus->destroyed == 1);
    NUI_CHECK(later->destroyed == 1);
    NUI_CHECK(Access::focusable_count(tree) == 1);
    inserted_focus->on_deactivate = {};
    inserted_focus->on_unmount = {};

    // Keep the next operation from retrying the failed insertion first. The
    // retained-only desired state now matches the rollback checkpoint, so focus
    // traversal/structure sync immediately exercises the surviving registry.
    visible.set(false);
    tree.focus_next(platform);
    NUI_CHECK(Access::focusable_count(tree) == 1);

    // A normal retry must mount/activate a fresh inserted subtree exactly once
    // and republish its focusable node after the failed attempt was destroyed.
    visible.set(true);
    NUI_CHECK(Access::reconcile_first(tree));
    tree.layout({160.0f, 80.0f});
    NUI_CHECK(Access::focusable_count(tree) == 2);
    NUI_CHECK(inserted_focus->mounts == 2);
    NUI_CHECK(inserted_focus->activates == (activation_failure ? 2 : 1));
    tree.focus_next(platform);

    tree.deactivate_focus(platform);
    tree.unmount();
}

void suite() {
    // A clip viewport prevents an overflowing child from painting outside it.
    {
        ui::UI tree{
            ui::Row{
                ui::Clip{
                    ui::Canvas{
                        ui::Size{40.0f, 40.0f},
                        [](ui::CanvasContext2D& g) {
                            g.fill_rect({-10.0f, 0.0f, 80.0f, 40.0f}, {1.0f, 0.0f, 0.0f, 1.0f});
                        }}},
                ui::Spacer{60.0f, 40.0f}}
                .gap(0.0f)};
        ui::HeadlessRenderer renderer{{100.0f, 60.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(red(renderer.pixel(20, 10)));
        NUI_CHECK(!red(renderer.pixel(60, 10)));
    }

    // Nested clips compose by intersection: the inner clip wins inside the outer viewport.
    {
        ui::UI tree{
            ui::Row{
                ui::Clip{
                    ui::Padding{10.0f,
                        ui::Clip{
                            ui::Canvas{
                                ui::Size{80.0f, 40.0f},
                                [](ui::CanvasContext2D& g) {
                                    g.fill_rect({-20.0f, -20.0f, 140.0f, 80.0f},
                                                {0.0f, 1.0f, 0.0f, 1.0f});
                                }}}}},
                ui::Spacer{20.0f, 60.0f}}
                .gap(0.0f)};
        ui::HeadlessRenderer renderer{{120.0f, 80.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(green(renderer.pixel(20, 20)));
        NUI_CHECK(!green(renderer.pixel(95, 20)));
        NUI_CHECK(!green(renderer.pixel(5, 20)));
    }

    // Hit testing follows the same clip chain. The child intentionally keeps a
    // 100px intrinsic width while the Flex+Clip viewport shrinks to 50px.
    {
        auto hit = std::make_shared<HitState>();
        ui::UI tree{
            ui::Row{
                ui::Flex{ui::Clip{Hit{hit}}}.shrink(1.0f),
                ui::Spacer{50.0f, 40.0f}}
                .gap(0.0f)};
        test::MockPlatform platform;
        tree.resize({100.0f, 40.0f});
        tree.activate(platform);
        NUI_CHECK(!hit->focus_bounds.empty());
        NUI_CHECK_NEAR(hit->focus_bounds.back().w, 100.0f, 0.001f);

        const auto clipped = tree.dispatch(
            test::pointer(ui::InputType::PointerDown, 75.0f, 20.0f), platform);
        NUI_CHECK(clipped == ui::EventResult::Ignored);
        NUI_CHECK(hit->pointer_down == 0);

        const auto inside = tree.dispatch(
            test::pointer(ui::InputType::PointerDown, 25.0f, 20.0f), platform);
        NUI_CHECK(inside == ui::EventResult::Handled);
        NUI_CHECK(hit->pointer_down == 1);
    }

    // T130: a descendant paint exception under a framework-owned Clip must not
    // leak SkCanvas save/clip state. The failed frame remains dirty and the same
    // UI + canvas can paint successfully on the next attempt.
    {
        auto state = std::make_shared<ThrowingPaintState>();
        ui::UI tree{ui::Clip{ThrowingPaint{state}}};
        tree.resize({40.0f, 40.0f});
        test::MockPlatform platform;

        const auto info = SkImageInfo::Make(
            64, 64, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
        auto surface = SkSurfaces::Raster(info);
        NUI_CHECK(surface != nullptr);
        auto* canvas = surface->getCanvas();
        NUI_CHECK(canvas != nullptr);
        const int baseline_save_count = canvas->getSaveCount();

        std::string propagated;
        try {
            tree.paint(*canvas, platform);
        } catch (const std::runtime_error& error) {
            propagated = error.what();
        }

        NUI_CHECK(propagated == "T130 throwing paint probe");
        NUI_CHECK(state->paints == 1);
        NUI_CHECK(canvas->getSaveCount() == baseline_save_count);
        NUI_CHECK(tree.paint_dirty());

        state->throw_on_paint = false;
        tree.paint(*canvas, platform);
        NUI_CHECK(state->paints == 2);
        NUI_CHECK(canvas->getSaveCount() == baseline_save_count);
        NUI_CHECK(!tree.paint_dirty());
    }

    dynamic_focus_registry_rollback_contract(false);
    dynamic_focus_registry_rollback_contract(true);
}

} // namespace

int main() { return test::run("clipping", &suite); }