#include "test_support.hpp"

namespace {

struct MountState {
    int mounts{};
    int unmounts{};
};

class MountProbeComponent final : public ui::Component {
public:
    explicit MountProbeComponent(std::shared_ptr<MountState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {24.0f, 16.0f};
    }

    void mount(ui::MountContext&) override { ++state_->mounts; }
    void unmount(ui::LifecycleContext&) override { ++state_->unmounts; }
    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<MountState> state_;
};

class MountProbe {
public:
    explicit MountProbe(std::shared_ptr<MountState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<MountProbeComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<MountState> state_;
};

struct ReentrantShowState {
    int pointer_downs{};
    bool mounted_reentrantly{};
    ui::OverlayHandle handle;
    std::function<ui::OverlayHandle()> show;
    std::shared_ptr<MountState> overlay_lifecycle;
};

class ReentrantShowComponent final : public ui::Component {
public:
    explicit ReentrantShowComponent(std::shared_ptr<ReentrantShowState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {96.0f, 48.0f};
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type != ui::InputType::PointerDown) return ui::EventResult::Ignored;

        ++state_->pointer_downs;
        state_->handle = state_->show();
        state_->mounted_reentrantly = state_->overlay_lifecycle->mounts != 0;
        return ui::EventResult::Handled;
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<ReentrantShowState> state_;
};

class ReentrantShowProbe {
public:
    explicit ReentrantShowProbe(std::shared_ptr<ReentrantShowState> state)
        : state_(std::move(state)) {}

    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<ReentrantShowComponent>(state);
            },
            {}};
    }

private:
    std::shared_ptr<ReentrantShowState> state_;
};

void suite() {
    test::MockPlatform platform;
    auto lifecycle = std::make_shared<MountState>();
    auto state = std::make_shared<ReentrantShowState>();
    state->overlay_lifecycle = lifecycle;

    ui::UI tree{ReentrantShowProbe{state}};
    tree.resize({96.0f, 48.0f});
    tree.activate(platform);

    state->show = [&tree, lifecycle] {
        ui::OverlaySpec overlay;
        overlay.placement = ui::OverlayPlacement::Center;
        overlay.content = ui::make_spec(MountProbe{lifecycle});
        return tree.show_overlay(std::move(overlay));
    };

    ui::InputEvent down;
    down.type = ui::InputType::PointerDown;
    down.position = {8.0f, 8.0f};
    NUI_CHECK(ui::handled(tree.dispatch(down, platform)));
    NUI_CHECK(state->pointer_downs == 1);
    NUI_CHECK(state->handle.valid());
    NUI_CHECK(lifecycle->mounts == 0);
    NUI_CHECK(!state->mounted_reentrantly);

    tree.resize({96.0f, 48.0f});
    NUI_CHECK(lifecycle->mounts == 1);

    NUI_CHECK(tree.close_overlay(state->handle));
    NUI_CHECK(!state->handle.valid());
    NUI_CHECK(lifecycle->unmounts == 0);
    tree.resize({96.0f, 48.0f});
    NUI_CHECK(lifecycle->unmounts == 1);
}

} // namespace

int main() { return test::run("t061_overlay_reentrant_show", &suite); }
