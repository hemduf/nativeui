#include "test_support.hpp"

#include <cmath>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

struct ContactState {
    std::vector<ui::PointerId> move;
    std::vector<ui::PointerId> up;
    std::vector<ui::PointerId> cancel;
    std::function<void(ui::InputContext&)> before_down_capture;
    std::function<void(ui::InputContext&)> after_down_capture;
    std::function<void(ui::InputContext&)> on_move_context;
    std::function<void()> on_up;
    std::function<void(ui::InputContext&)> on_up_context;
    std::function<void()> on_cancel;
    bool throw_on_down{};
    bool throw_on_up{};
    bool throw_on_cancel{};
};

class ContactProbeComponent final : public ui::Component {
public:
    explicit ContactProbeComponent(std::shared_ptr<ContactState> state)
        : state_(std::move(state)) {}
    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {100.0f, 100.0f};
    }
    ui::EventResult input(const ui::InputEvent& event, ui::InputContext& context) override {
        switch (event.type) {
        case ui::InputType::PointerDown:
            if (state_->before_down_capture) state_->before_down_capture(context);
            context.capture_pointer();
            if (state_->after_down_capture) state_->after_down_capture(context);
            if (state_->throw_on_down) {
                state_->throw_on_down = false;
                throw std::runtime_error("contact down fault");
            }
            return ui::EventResult::Handled;
        case ui::InputType::PointerMove:
            state_->move.push_back(event.pointer.id);
            if (state_->on_move_context) state_->on_move_context(context);
            return ui::EventResult::Handled;
        case ui::InputType::PointerUp:
            state_->up.push_back(event.pointer.id);
            if (state_->on_up) state_->on_up();
            if (state_->on_up_context) state_->on_up_context(context);
            if (state_->throw_on_up) {
                state_->throw_on_up = false;
                throw std::runtime_error("contact up fault");
            }
            return ui::EventResult::Handled;
        case ui::InputType::PointerCancel:
            state_->cancel.push_back(event.pointer.id);
            if (state_->on_cancel) state_->on_cancel();
            if (state_->throw_on_cancel) {
                state_->throw_on_cancel = false;
                throw std::runtime_error("contact cancel fault");
            }
            return ui::EventResult::Handled;
        default:
            return ui::EventResult::Ignored;
        }
    }
    void paint(ui::PaintContext&) const override {}
private:
    std::shared_ptr<ContactState> state_;
};

class ContactProbe {
public:
    explicit ContactProbe(std::shared_ptr<ContactState> state) : state_(std::move(state)) {}
    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{
            [state = std::move(state)] {
                return std::make_unique<ContactProbeComponent>(state);
            },
            {}};
    }
private:
    std::shared_ptr<ContactState> state_;
};

class SplitComponent final : public ui::Component {
public:
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {200.0f, 100.0f};
    }
    void layout_children(ui::Rect bounds,
                         const std::vector<ui::ChildMetrics>&,
                         std::vector<ui::ChildPlacement>& placements) const override {
        if (placements.size() < 2U) return;
        const float half = bounds.w * 0.5f;
        placements[0].bounds = {bounds.x, bounds.y, half, bounds.h};
        placements[1].bounds = {bounds.x + half, bounds.y, bounds.w - half, bounds.h};
    }
    void paint(ui::PaintContext&) const override {}
};

class Split {
public:
    Split(ContactProbe first, ContactProbe second) {
        children_.push_back(std::move(first).spec());
        children_.push_back(std::move(second).spec());
    }
    ui::Spec spec() && {
        return ui::Spec{[] { return std::make_unique<SplitComponent>(); },
                        std::move(children_)};
    }
private:
    std::vector<ui::Spec> children_;
};

ui::InputEvent pointer(ui::InputType type,
                       ui::PointerId id,
                       float x,
                       float y,
                       ui::PointerType source = ui::PointerType::Touch) {
    ui::InputEvent event{};
    event.type = type;
    event.position = {x, y};
    event.pointer.id = id;
    event.pointer.type = source;
    event.pointer.primary = id == 1U;
    event.pointer.pressure = 0.5f;
    event.pointer.contact_size = {12.0f, 10.0f};
    return event;
}

void suite() {
    {
        const ui::InputEvent legacy{};
        NUI_CHECK(legacy.pointer.id == 0U);
        NUI_CHECK(legacy.pointer.type == ui::PointerType::Unknown);
        NUI_CHECK(!legacy.pointer.tracked());
        NUI_CHECK(legacy.pointer.hover_capable());
        NUI_CHECK(std::isnan(legacy.pointer.pressure));
        NUI_CHECK(std::isnan(legacy.pointer.contact_size.w));
        NUI_CHECK(std::isnan(legacy.pointer.contact_size.h));

        ui::PointerContact touch_contact{};
        touch_contact.id = 9U;
        touch_contact.type = ui::PointerType::Touch;
        NUI_CHECK(touch_contact.tracked());
        NUI_CHECK(!touch_contact.hover_capable());

        ui::PointerContact mouse_contact{};
        mouse_contact.type = ui::PointerType::Mouse;
        NUI_CHECK(mouse_contact.hover_capable());
    }

    {
        auto left = std::make_shared<ContactState>();
        auto right = std::make_shared<ContactState>();
        test::MockPlatform platform;
        ui::UI tree{Split{ContactProbe{left}, ContactProbe{right}}};
        tree.resize({200.0f, 100.0f});
        tree.activate(platform);

        NUI_CHECK(tree.dispatch(pointer(ui::InputType::PointerDown, 1U, 25.0f, 50.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(tree.dispatch(pointer(ui::InputType::PointerDown, 2U, 175.0f, 50.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(platform.pointer_capture_begin_count == 0);

        (void)tree.dispatch(pointer(ui::InputType::PointerMove, 1U, 175.0f, 50.0f), platform);
        (void)tree.dispatch(pointer(ui::InputType::PointerMove, 2U, 25.0f, 50.0f), platform);
        NUI_CHECK(left->move.size() == 1U && left->move.back() == 1U);
        NUI_CHECK(right->move.size() == 1U && right->move.back() == 2U);

        (void)tree.dispatch(pointer(ui::InputType::PointerUp, 1U, 175.0f, 50.0f), platform);
        (void)tree.dispatch(pointer(ui::InputType::PointerMove, 2U, 25.0f, 50.0f), platform);
        NUI_CHECK(left->up.size() == 1U && left->up.back() == 1U);
        NUI_CHECK(right->move.size() == 2U);

        (void)tree.dispatch(pointer(ui::InputType::PointerCancel, 2U, 25.0f, 50.0f), platform);
        NUI_CHECK(right->cancel.size() == 1U && right->cancel.back() == 2U);
        NUI_CHECK(platform.pointer_capture_end_count == 0);
    }

    {
        auto left = std::make_shared<ContactState>();
        auto right = std::make_shared<ContactState>();
        test::MockPlatform platform;
        ui::UI tree{Split{ContactProbe{left}, ContactProbe{right}}};
        tree.resize({200.0f, 100.0f});
        tree.activate(platform);

        (void)tree.dispatch(
            pointer(ui::InputType::PointerDown, 0U, 25.0f, 50.0f, ui::PointerType::Mouse),
            platform);
        NUI_CHECK(platform.pointer_capture_begin_count == 1);
        (void)tree.dispatch(
            pointer(ui::InputType::PointerMove, 0U, 175.0f, 50.0f, ui::PointerType::Mouse),
            platform);
        NUI_CHECK(left->move.size() == 1U);
        (void)tree.dispatch(
            pointer(ui::InputType::PointerUp, 0U, 175.0f, 50.0f, ui::PointerType::Mouse),
            platform);
        NUI_CHECK(platform.pointer_capture_end_count == 1);
    }

    {
        auto left = std::make_shared<ContactState>();
        auto right = std::make_shared<ContactState>();
        left->throw_on_cancel = true;
        test::MockPlatform platform;
        ui::UI tree{Split{ContactProbe{left}, ContactProbe{right}}};
        tree.resize({200.0f, 100.0f});
        tree.activate(platform);

        (void)tree.dispatch(pointer(ui::InputType::PointerDown, 3U, 25.0f, 50.0f), platform);
        (void)tree.dispatch(pointer(ui::InputType::PointerDown, 4U, 175.0f, 50.0f), platform);
        bool threw = false;
        try {
            (void)tree.dispatch(
                pointer(ui::InputType::PointerCancel, 3U, 25.0f, 50.0f), platform);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);

        (void)tree.dispatch(pointer(ui::InputType::PointerMove, 4U, 25.0f, 50.0f), platform);
        NUI_CHECK(right->move.size() == 1U && right->move.back() == 4U);
        (void)tree.dispatch(pointer(ui::InputType::PointerUp, 4U, 25.0f, 50.0f), platform);
    }

    // A terminal callback may synchronously start a newer interaction that
    // reuses the same platform pointer ID. Cleanup for the older PointerUp must
    // only retire the capture/interaction it observed on entry.
    {
        auto left = std::make_shared<ContactState>();
        auto right = std::make_shared<ContactState>();
        test::MockPlatform platform;
        ui::UI tree{Split{ContactProbe{left}, ContactProbe{right}}};
        tree.resize({200.0f, 100.0f});
        tree.activate(platform);

        (void)tree.dispatch(pointer(ui::InputType::PointerDown, 5U, 25.0f, 50.0f), platform);
        left->on_up = [&] {
            (void)tree.dispatch(
                pointer(ui::InputType::PointerDown, 5U, 175.0f, 50.0f), platform);
        };
        (void)tree.dispatch(pointer(ui::InputType::PointerUp, 5U, 25.0f, 50.0f), platform);
        left->on_up = {};

        (void)tree.dispatch(pointer(ui::InputType::PointerMove, 5U, 25.0f, 50.0f), platform);
        NUI_CHECK(right->move.size() == 1U && right->move.back() == 5U);
        (void)tree.dispatch(pointer(ui::InputType::PointerUp, 5U, 25.0f, 50.0f), platform);
    }

    // The same newer interaction must survive when the older PointerUp throws
    // after recursively starting it.
    {
        auto left = std::make_shared<ContactState>();
        auto right = std::make_shared<ContactState>();
        test::MockPlatform platform;
        ui::UI tree{Split{ContactProbe{left}, ContactProbe{right}}};
        tree.resize({200.0f, 100.0f});
        tree.activate(platform);

        (void)tree.dispatch(pointer(ui::InputType::PointerDown, 6U, 25.0f, 50.0f), platform);
        left->on_up = [&] {
            (void)tree.dispatch(
                pointer(ui::InputType::PointerDown, 6U, 175.0f, 50.0f), platform);
        };
        left->throw_on_up = true;

        bool threw = false;
        try {
            (void)tree.dispatch(
                pointer(ui::InputType::PointerUp, 6U, 25.0f, 50.0f), platform);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        left->on_up = {};

        (void)tree.dispatch(pointer(ui::InputType::PointerMove, 6U, 25.0f, 50.0f), platform);
        NUI_CHECK(right->move.size() == 1U && right->move.back() == 6U);
        (void)tree.dispatch(pointer(ui::InputType::PointerUp, 6U, 25.0f, 50.0f), platform);
    }

    // Cancellation uses the same token-matched cleanup. A nested Down with the
    // same stable ID remains authoritative even if the older cancel then throws.
    {
        auto left = std::make_shared<ContactState>();
        auto right = std::make_shared<ContactState>();
        test::MockPlatform platform;
        ui::UI tree{Split{ContactProbe{left}, ContactProbe{right}}};
        tree.resize({200.0f, 100.0f});
        tree.activate(platform);

        (void)tree.dispatch(pointer(ui::InputType::PointerDown, 7U, 25.0f, 50.0f), platform);
        bool reenter_once = true;
        left->on_cancel = [&] {
            if (!reenter_once) return;
            reenter_once = false;
            (void)tree.dispatch(
                pointer(ui::InputType::PointerDown, 7U, 175.0f, 50.0f), platform);
            left->throw_on_cancel = true;
        };

        bool threw = false;
        try {
            (void)tree.dispatch(
                pointer(ui::InputType::PointerCancel, 7U, 25.0f, 50.0f),
                platform);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        left->on_cancel = {};
        NUI_CHECK(left->cancel.size() == 1U && left->cancel.back() == 7U);

        (void)tree.dispatch(pointer(ui::InputType::PointerMove, 7U, 25.0f, 50.0f), platform);
        NUI_CHECK(right->move.size() == 1U && right->move.back() == 7U);
        (void)tree.dispatch(pointer(ui::InputType::PointerUp, 7U, 25.0f, 50.0f), platform);
    }

    // A per-contact cancellation may request global pointer teardown. The
    // contact whose callback is already running must not receive a duplicate
    // PointerCancel, while sibling captures are still cancelled exactly once.
    {
        auto left = std::make_shared<ContactState>();
        auto right = std::make_shared<ContactState>();
        test::MockPlatform platform;
        ui::UI tree{Split{ContactProbe{left}, ContactProbe{right}}};
        tree.resize({200.0f, 100.0f});
        tree.activate(platform);

        (void)tree.dispatch(
            pointer(ui::InputType::PointerDown, 15U, 25.0f, 50.0f), platform);
        (void)tree.dispatch(
            pointer(ui::InputType::PointerDown, 16U, 175.0f, 50.0f), platform);

        left->on_cancel = [&] {
            (void)tree.cancel_pointer(platform);
        };

        (void)tree.dispatch(
            pointer(ui::InputType::PointerCancel, 15U, 25.0f, 50.0f), platform);
        left->on_cancel = {};

        NUI_CHECK(left->cancel.size() == 1U && left->cancel.back() == 15U);
        NUI_CHECK(right->cancel.size() == 1U && right->cancel.back() == 16U);

        (void)tree.dispatch(
            pointer(ui::InputType::PointerDown, 17U, 175.0f, 50.0f), platform);
        (void)tree.dispatch(
            pointer(ui::InputType::PointerMove, 17U, 25.0f, 50.0f), platform);
        NUI_CHECK(right->move.size() == 1U && right->move.back() == 17U);
        (void)tree.dispatch(
            pointer(ui::InputType::PointerUp, 17U, 25.0f, 50.0f), platform);
    }

    // A PointerDown that first cancels a stale same-ID capture must also yield
    // to a newer same-ID Down delivered reentrantly by that cancellation.
    {
        auto left = std::make_shared<ContactState>();
        auto right = std::make_shared<ContactState>();
        test::MockPlatform platform;
        ui::UI tree{Split{ContactProbe{left}, ContactProbe{right}}};
        tree.resize({200.0f, 100.0f});
        tree.activate(platform);

        (void)tree.dispatch(
            pointer(ui::InputType::PointerDown, 18U, 25.0f, 50.0f), platform);

        bool reenter_once = true;
        left->on_cancel = [&] {
            if (!reenter_once) return;
            reenter_once = false;
            (void)tree.dispatch(
                pointer(ui::InputType::PointerDown, 18U, 175.0f, 50.0f),
                platform);
        };

        // This Down finds the older left capture and cancels it first. The
        // nested right Down from on_cancel is newer and must remain authoritative.
        (void)tree.dispatch(
            pointer(ui::InputType::PointerDown, 18U, 25.0f, 50.0f), platform);
        left->on_cancel = {};

        left->move.clear();
        right->move.clear();
        (void)tree.dispatch(
            pointer(ui::InputType::PointerMove, 18U, 25.0f, 50.0f), platform);
        NUI_CHECK(left->move.empty());
        NUI_CHECK(right->move.size() == 1U && right->move.back() == 18U);
        (void)tree.dispatch(
            pointer(ui::InputType::PointerUp, 18U, 25.0f, 50.0f), platform);
    }

    // An older PointerDown frame must not capture after a nested newer Down
    // reused the same platform ID. The newer contact owns the generation.
    {
        auto left = std::make_shared<ContactState>();
        auto right = std::make_shared<ContactState>();
        test::MockPlatform platform;
        ui::UI tree{Split{ContactProbe{left}, ContactProbe{right}}};
        tree.resize({200.0f, 100.0f});
        tree.activate(platform);

        bool reenter_once = true;
        left->before_down_capture = [&](ui::InputContext&) {
            if (!reenter_once) return;
            reenter_once = false;
            (void)tree.dispatch(
                pointer(ui::InputType::PointerDown, 10U, 175.0f, 50.0f),
                platform);
        };

        (void)tree.dispatch(
            pointer(ui::InputType::PointerDown, 10U, 25.0f, 50.0f), platform);
        left->before_down_capture = {};

        (void)tree.dispatch(
            pointer(ui::InputType::PointerMove, 10U, 25.0f, 50.0f), platform);
        NUI_CHECK(left->move.empty());
        NUI_CHECK(right->move.size() == 1U && right->move.back() == 10U);
        (void)tree.dispatch(
            pointer(ui::InputType::PointerUp, 10U, 25.0f, 50.0f), platform);
    }

    // A stale terminal InputContext must likewise be unable to release a newer
    // same-ID capture created recursively by its callback.
    {
        auto left = std::make_shared<ContactState>();
        auto right = std::make_shared<ContactState>();
        test::MockPlatform platform;
        ui::UI tree{Split{ContactProbe{left}, ContactProbe{right}}};
        tree.resize({200.0f, 100.0f});
        tree.activate(platform);

        (void)tree.dispatch(
            pointer(ui::InputType::PointerDown, 11U, 25.0f, 50.0f), platform);
        left->on_up_context = [&](ui::InputContext& context) {
            (void)tree.dispatch(
                pointer(ui::InputType::PointerDown, 11U, 25.0f, 50.0f),
                platform);
            context.release_pointer();
        };

        (void)tree.dispatch(
            pointer(ui::InputType::PointerUp, 11U, 25.0f, 50.0f), platform);
        left->on_up_context = {};

        (void)tree.dispatch(
            pointer(ui::InputType::PointerMove, 11U, 175.0f, 50.0f), platform);
        NUI_CHECK(left->move.size() == 1U && left->move.back() == 11U);
        NUI_CHECK(right->move.empty());
        (void)tree.dispatch(
            pointer(ui::InputType::PointerUp, 11U, 175.0f, 50.0f), platform);
    }

    // A still-live outer InputContext may be used while another contact's
    // callback is on the stack. Its release must keep the outer pointer ID.
    {
        auto left = std::make_shared<ContactState>();
        auto right = std::make_shared<ContactState>();
        test::MockPlatform platform;
        ui::UI tree{Split{ContactProbe{left}, ContactProbe{right}}};
        tree.resize({200.0f, 100.0f});
        tree.activate(platform);

        (void)tree.dispatch(pointer(ui::InputType::PointerDown, 21U, 25.0f, 50.0f), platform);
        left->on_move_context = [&](ui::InputContext& outer) {
            right->after_down_capture = [&](ui::InputContext&) {
                outer.release_pointer();
            };
            (void)tree.dispatch(
                pointer(ui::InputType::PointerDown, 22U, 175.0f, 50.0f), platform);
            right->after_down_capture = {};
        };
        (void)tree.dispatch(pointer(ui::InputType::PointerMove, 21U, 25.0f, 50.0f), platform);
        left->on_move_context = {};
        left->move.clear();
        right->move.clear();

        (void)tree.dispatch(pointer(ui::InputType::PointerMove, 21U, 175.0f, 50.0f), platform);
        (void)tree.dispatch(pointer(ui::InputType::PointerMove, 22U, 25.0f, 50.0f), platform);
        NUI_CHECK(left->move.empty());
        NUI_CHECK(right->move.size() == 2U &&
                  right->move[0] == 21U && right->move[1] == 22U);
        (void)tree.dispatch(pointer(ui::InputType::PointerUp, 21U, 175.0f, 50.0f), platform);
        (void)tree.dispatch(pointer(ui::InputType::PointerUp, 22U, 25.0f, 50.0f), platform);
    }

    // If a nested Down reuses the pointer ID, an outer context must retain
    // its older generation even while the newer callback is still active.
    {
        auto left = std::make_shared<ContactState>();
        auto right = std::make_shared<ContactState>();
        test::MockPlatform platform;
        ui::UI tree{Split{ContactProbe{left}, ContactProbe{right}}};
        tree.resize({200.0f, 100.0f});
        tree.activate(platform);

        (void)tree.dispatch(pointer(ui::InputType::PointerDown, 23U, 25.0f, 50.0f), platform);
        left->on_move_context = [&](ui::InputContext& outer) {
            right->after_down_capture = [&](ui::InputContext&) {
                outer.capture_pointer();
            };
            (void)tree.dispatch(
                pointer(ui::InputType::PointerDown, 23U, 175.0f, 50.0f), platform);
            right->after_down_capture = {};
        };
        (void)tree.dispatch(pointer(ui::InputType::PointerMove, 23U, 25.0f, 50.0f), platform);
        left->on_move_context = {};
        left->move.clear();
        right->move.clear();

        (void)tree.dispatch(pointer(ui::InputType::PointerMove, 23U, 25.0f, 50.0f), platform);
        NUI_CHECK(left->move.empty());
        NUI_CHECK(right->move.size() == 1U && right->move.back() == 23U);
        (void)tree.dispatch(pointer(ui::InputType::PointerUp, 23U, 25.0f, 50.0f), platform);
    }

    // A nested terminal event must retire its contact even if an older Move
    // context attempts to recapture that same contact during the Up callback.
    {
        auto left = std::make_shared<ContactState>();
        auto right = std::make_shared<ContactState>();
        test::MockPlatform platform;
        ui::UI tree{Split{ContactProbe{left}, ContactProbe{right}}};
        tree.resize({200.0f, 100.0f});
        tree.activate(platform);

        (void)tree.dispatch(pointer(ui::InputType::PointerDown, 24U, 25.0f, 50.0f), platform);
        left->on_move_context = [&](ui::InputContext& outer) {
            left->on_up_context = [&](ui::InputContext&) {
                outer.capture_pointer();
            };
            (void)tree.dispatch(
                pointer(ui::InputType::PointerUp, 24U, 25.0f, 50.0f), platform);
            left->on_up_context = {};
        };
        (void)tree.dispatch(pointer(ui::InputType::PointerMove, 24U, 25.0f, 50.0f), platform);
        left->on_move_context = {};
        left->move.clear();
        right->move.clear();

        (void)tree.dispatch(pointer(ui::InputType::PointerMove, 24U, 175.0f, 50.0f), platform);
        NUI_CHECK(left->move.empty());
        NUI_CHECK(right->move.size() == 1U && right->move.back() == 24U);

        (void)tree.dispatch(pointer(ui::InputType::PointerDown, 24U, 175.0f, 50.0f), platform);
        (void)tree.dispatch(pointer(ui::InputType::PointerUp, 24U, 175.0f, 50.0f), platform);
    }

    // Reentrant Move may renew capture for the same contact while its Up is
    // running. Terminal cleanup must retire that generation, not just the
    // capture operation that happened to be current on Up entry.
    {
        auto left = std::make_shared<ContactState>();
        auto right = std::make_shared<ContactState>();
        test::MockPlatform platform;
        ui::UI tree{Split{ContactProbe{left}, ContactProbe{right}}};
        tree.resize({200.0f, 100.0f});
        tree.activate(platform);

        (void)tree.dispatch(pointer(ui::InputType::PointerDown, 25U, 25.0f, 50.0f), platform);
        left->on_move_context = [](ui::InputContext& context) { context.capture_pointer(); };
        left->on_up_context = [&](ui::InputContext&) {
            (void)tree.dispatch(
                pointer(ui::InputType::PointerMove, 25U, 25.0f, 50.0f), platform);
        };
        (void)tree.dispatch(pointer(ui::InputType::PointerUp, 25U, 25.0f, 50.0f), platform);
        left->on_move_context = {};
        left->on_up_context = {};
        left->move.clear();
        right->move.clear();

        (void)tree.dispatch(pointer(ui::InputType::PointerMove, 25U, 175.0f, 50.0f), platform);
        NUI_CHECK(left->move.empty());
        NUI_CHECK(right->move.size() == 1U && right->move.back() == 25U);
    }

    // Failed Down cleanup owns every capture from its interaction generation,
    // including a capture renewed by a nested Move before the throw.
    {
        auto left = std::make_shared<ContactState>();
        auto right = std::make_shared<ContactState>();
        test::MockPlatform platform;
        ui::UI tree{Split{ContactProbe{left}, ContactProbe{right}}};
        tree.resize({200.0f, 100.0f});
        tree.activate(platform);

        left->on_move_context = [](ui::InputContext& context) { context.capture_pointer(); };
        left->after_down_capture = [&](ui::InputContext&) {
            (void)tree.dispatch(
                pointer(ui::InputType::PointerMove, 26U, 25.0f, 50.0f), platform);
        };
        left->throw_on_down = true;
        bool threw = false;
        try {
            (void)tree.dispatch(
                pointer(ui::InputType::PointerDown, 26U, 25.0f, 50.0f), platform);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        left->on_move_context = {};
        left->after_down_capture = {};
        left->move.clear();
        right->move.clear();

        (void)tree.dispatch(pointer(ui::InputType::PointerMove, 26U, 175.0f, 50.0f), platform);
        NUI_CHECK(left->move.empty());
        NUI_CHECK(right->move.size() == 1U && right->move.back() == 26U);
        (void)tree.dispatch(pointer(ui::InputType::PointerDown, 26U, 175.0f, 50.0f), platform);
        (void)tree.dispatch(pointer(ui::InputType::PointerUp, 26U, 175.0f, 50.0f), platform);
    }

    // Reentrant cancel_pointer() is idempotent for the active teardown pass:
    // every contact receives exactly one cancellation and the tree recovers.
    {
        auto left = std::make_shared<ContactState>();
        auto right = std::make_shared<ContactState>();
        test::MockPlatform platform;
        ui::UI tree{Split{ContactProbe{left}, ContactProbe{right}}};
        tree.resize({200.0f, 100.0f});
        tree.activate(platform);

        (void)tree.dispatch(
            pointer(ui::InputType::PointerDown, 12U, 25.0f, 50.0f), platform);
        (void)tree.dispatch(
            pointer(ui::InputType::PointerDown, 13U, 175.0f, 50.0f), platform);

        bool reenter_once = true;
        left->on_cancel = [&] {
            if (!reenter_once) return;
            reenter_once = false;
            (void)tree.cancel_pointer(platform);
        };

        (void)tree.cancel_pointer(platform);
        left->on_cancel = {};
        NUI_CHECK(left->cancel.size() == 1U && left->cancel.back() == 12U);
        NUI_CHECK(right->cancel.size() == 1U && right->cancel.back() == 13U);

        (void)tree.dispatch(
            pointer(ui::InputType::PointerDown, 14U, 175.0f, 50.0f), platform);
        (void)tree.dispatch(
            pointer(ui::InputType::PointerMove, 14U, 25.0f, 50.0f), platform);
        NUI_CHECK(right->move.size() == 1U && right->move.back() == 14U);
        (void)tree.dispatch(
            pointer(ui::InputType::PointerUp, 14U, 25.0f, 50.0f), platform);
    }

    // cancel_pointer() is a global teardown barrier: callbacks may re-enter,
    // but they cannot publish a fresh capture that keeps the teardown loop
    // alive. The barrier must also restore after a throwing cancellation.
    {
        auto left = std::make_shared<ContactState>();
        auto right = std::make_shared<ContactState>();
        test::MockPlatform platform;
        ui::UI tree{Split{ContactProbe{left}, ContactProbe{right}}};
        tree.resize({200.0f, 100.0f});
        tree.activate(platform);

        (void)tree.dispatch(pointer(ui::InputType::PointerDown, 8U, 25.0f, 50.0f), platform);
        left->on_cancel = [&] {
            (void)tree.dispatch(
                pointer(ui::InputType::PointerDown, 8U, 175.0f, 50.0f), platform);
        };
        left->throw_on_cancel = true;

        bool threw = false;
        try {
            (void)tree.cancel_pointer(platform);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        NUI_CHECK(threw);
        left->on_cancel = {};

        (void)tree.dispatch(pointer(ui::InputType::PointerMove, 8U, 25.0f, 50.0f), platform);
        NUI_CHECK(left->move.size() == 1U);
        NUI_CHECK(right->move.empty());

        (void)tree.dispatch(pointer(ui::InputType::PointerDown, 9U, 175.0f, 50.0f), platform);
        (void)tree.dispatch(pointer(ui::InputType::PointerMove, 9U, 25.0f, 50.0f), platform);
        NUI_CHECK(right->move.size() == 1U && right->move.back() == 9U);
        (void)tree.dispatch(pointer(ui::InputType::PointerUp, 9U, 25.0f, 50.0f), platform);
    }

    {
        auto a_left = std::make_shared<ContactState>();
        auto a_right = std::make_shared<ContactState>();
        auto b_left = std::make_shared<ContactState>();
        auto b_right = std::make_shared<ContactState>();
        test::MockPlatform platform_a;
        test::MockPlatform platform_b;

        auto a = std::make_unique<ui::UI>(
            Split{ContactProbe{a_left}, ContactProbe{a_right}});
        ui::UI b{Split{ContactProbe{b_left}, ContactProbe{b_right}}};
        a->resize({200.0f, 100.0f});
        b.resize({200.0f, 100.0f});
        a->activate(platform_a);
        b.activate(platform_b);

        (void)a->dispatch(pointer(ui::InputType::PointerDown, 1U, 25.0f, 50.0f), platform_a);
        (void)b.dispatch(pointer(ui::InputType::PointerDown, 1U, 175.0f, 50.0f), platform_b);
        a.reset();

        (void)b.dispatch(pointer(ui::InputType::PointerMove, 1U, 25.0f, 50.0f), platform_b);
        NUI_CHECK(b_right->move.size() == 1U && b_right->move.back() == 1U);
        (void)b.dispatch(pointer(ui::InputType::PointerUp, 1U, 25.0f, 50.0f), platform_b);
    }
}

} // namespace

int main() {
    return test::run("multitouch pointer routing", suite);
}
