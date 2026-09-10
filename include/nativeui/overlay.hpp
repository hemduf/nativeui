#pragma once

#include <nativeui/component_base.hpp>
#include <nativeui/detail/dynamic_source.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ui {

enum class OverlayMode {
    NonModal,
    Modal,
};

enum class OverlayPointerPolicy {
    Normal,
    Ignore,
};

// X11/Xlib exposes process-wide preprocessor macros named `Above` and `Below`.
// NativeUI public headers must remain consumable after Xlib headers, so the
// anchor-relative names deliberately avoid those unqualifiable macro tokens.
enum class OverlayPlacement {
    AnchorBelow,
    AnchorAbove,
    AnchorRight,
    AnchorLeft,
    Center,
    Auto,
};

namespace detail {
struct OverlayOwnerToken final {};
struct OverlayLifetimeToken final {};
struct OverlayState;
} // namespace detail

class OverlayHandle {
public:
    OverlayHandle() = default;

    [[nodiscard]] bool valid() const noexcept {
        return id_ != 0 && !owner_.expired() && !lifetime_.expired();
    }

    explicit operator bool() const noexcept { return valid(); }

    [[nodiscard]] bool operator==(const OverlayHandle& other) const noexcept {
        return id_ == other.id_ && !owner_.owner_before(other.owner_) &&
               !other.owner_.owner_before(owner_);
    }

private:
    friend struct detail::OverlayState;

    OverlayHandle(std::weak_ptr<const detail::OverlayOwnerToken> owner,
                  std::weak_ptr<const detail::OverlayLifetimeToken> lifetime,
                  std::uint64_t id) noexcept
        : owner_(std::move(owner)), lifetime_(std::move(lifetime)), id_(id) {}

    std::weak_ptr<const detail::OverlayOwnerToken> owner_;
    std::weak_ptr<const detail::OverlayLifetimeToken> lifetime_;
    std::uint64_t id_{};
};

struct OverlaySpec {
    OverlayMode mode{OverlayMode::NonModal};
    OverlayPointerPolicy pointer_policy{OverlayPointerPolicy::Normal};
    std::optional<NodeId> anchor;
    OverlayPlacement placement{OverlayPlacement::Auto};
    bool dismiss_on_escape{};
    bool dismiss_on_outside_pointer_down{};
    Spec content;
};

namespace detail {

struct OverlayEntry {
    std::uint64_t id{};
    OverlaySpec spec;
    std::shared_ptr<const OverlayLifetimeToken> lifetime;
    std::optional<Rect> anchor_bounds;
};

struct OverlayState {
    std::shared_ptr<const OverlayOwnerToken> owner{
        std::make_shared<const OverlayOwnerToken>()};
    std::uint64_t next_id{1};
    std::vector<OverlayEntry> entries;
    std::function<void()> structural_invalidator;

    void invalidate_structure() const {
        if (structural_invalidator) structural_invalidator();
    }

    [[nodiscard]] OverlayHandle show(OverlaySpec overlay) {
        if (overlay.mode == OverlayMode::Modal &&
            overlay.pointer_policy == OverlayPointerPolicy::Ignore) {
            return {};
        }
        if (next_id == 0) return {};

        const auto id = next_id;
        if (next_id == std::numeric_limits<std::uint64_t>::max()) next_id = 0;
        else ++next_id;

        auto lifetime = std::make_shared<const OverlayLifetimeToken>();
        entries.push_back(OverlayEntry{id, std::move(overlay), lifetime, std::nullopt});
        invalidate_structure();
        return OverlayHandle{owner, lifetime, id};
    }

    bool close(OverlayHandle handle) {
        const auto handle_owner = handle.owner_.lock();
        const auto lifetime = handle.lifetime_.lock();
        if (!handle_owner || handle_owner != owner || !lifetime || handle.id_ == 0) {
            return false;
        }

        const auto it = std::find_if(entries.begin(), entries.end(), [&](const OverlayEntry& entry) {
            return entry.id == handle.id_ && entry.lifetime == lifetime;
        });
        if (it == entries.end()) return false;
        entries.erase(it);
        invalidate_structure();
        return true;
    }

    bool close_id(std::uint64_t id) {
        const auto it = std::find_if(entries.begin(), entries.end(), [id](const OverlayEntry& entry) {
            return entry.id == id;
        });
        if (it == entries.end()) return false;
        entries.erase(it);
        invalidate_structure();
        return true;
    }
};

class OverlayEntryComponent final : public Component {
public:
    OverlayEntryComponent(OverlayMode mode, OverlayPointerPolicy pointer_policy)
        : mode_(mode), pointer_policy_(pointer_policy) {}

    [[nodiscard]] bool focusable() const noexcept override {
        // A modal with no focusable content still needs a focus target so the
        // previously focused root cannot keep receiving keyboard activation.
        return mode_ == OverlayMode::Modal;
    }

    [[nodiscard]] bool pointer_targetable() const noexcept override {
        return pointer_policy_ == OverlayPointerPolicy::Normal;
    }

    [[nodiscard]] bool is_focus_scope() const noexcept override {
        return mode_ == OverlayMode::Modal;
    }

    [[nodiscard]] bool focus_scope_active() const noexcept override {
        return mode_ == OverlayMode::Modal;
    }

    [[nodiscard]] bool focus_scope_traps() const noexcept override {
        return mode_ == OverlayMode::Modal;
    }

    [[nodiscard]] std::size_t focus_scope_default_index() const noexcept override {
        // Index zero is this wrapper. Prefer the first real focusable child,
        // with default_focus_for_scope() falling back to the wrapper itself.
        return 1;
    }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }

    void layout_children(Rect bounds,
                         const std::vector<ChildMetrics>&,
                         std::vector<ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

    EventResult input(const InputEvent& event, InputContext&) override {
        const bool pointer_event = event.type == InputType::PointerDown ||
                                   event.type == InputType::PointerMove ||
                                   event.type == InputType::PointerUp ||
                                   event.type == InputType::PointerCancel ||
                                   event.type == InputType::PointerWheel;
        if (pointer_event && pointer_policy_ == OverlayPointerPolicy::Normal) {
            return EventResult::Handled;
        }

        const bool keyboard_event = event.type == InputType::KeyDown ||
                                    event.type == InputType::KeyUp ||
                                    event.type == InputType::Command ||
                                    event.type == InputType::TextInput ||
                                    event.type == InputType::Composition;
        if (keyboard_event && mode_ == OverlayMode::Modal) return EventResult::Handled;
        return EventResult::Ignored;
    }

    void paint(PaintContext&) const override {}

private:
    OverlayMode mode_{};
    OverlayPointerPolicy pointer_policy_{};
};

class OverlayHostComponent final : public Component, public DynamicChildrenSource {
public:
    OverlayHostComponent(std::shared_ptr<OverlayState> state, std::shared_ptr<const Spec> root)
        : state_(std::move(state)), root_(std::move(root)) {}

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }

    [[nodiscard]] Constraints child_constraints(
        std::size_t child_index, const Constraints& constraints) const override {
        return child_index == 0 ? constraints : constraints.loosen();
    }

    void layout_children(Rect bounds,
                         const std::vector<ChildMetrics>& children,
                         std::vector<ChildPlacement>& placements) const override {
        if (placements.empty()) return;
        placements.front().bounds = bounds;
        const auto overlay_count = std::min(state_->entries.size(), placements.size() - 1);
        for (std::size_t i = 0; i < overlay_count; ++i) {
            const auto size = children[i + 1].preferred;
            placements[i + 1].bounds = Rect{
                bounds.x + (bounds.w - size.w) * 0.5f,
                bounds.y + (bounds.h - size.h) * 0.5f,
                size.w,
                size.h};
        }
    }

    [[nodiscard]] std::vector<std::string> desired_keys() const override {
        std::vector<std::string> keys;
        keys.reserve(state_->entries.size() + 1);
        keys.emplace_back("root");
        for (const auto& entry : state_->entries) {
            keys.push_back("overlay:" + std::to_string(entry.id));
        }
        return keys;
    }

    [[nodiscard]] std::vector<DynamicChildSpec> desired_children() const override {
        std::vector<DynamicChildSpec> children;
        children.reserve(state_->entries.size() + 1);
        children.push_back(DynamicChildSpec{"root", *root_});
        for (const auto& entry : state_->entries) {
            Spec wrapper{
                [mode = entry.spec.mode, pointer_policy = entry.spec.pointer_policy] {
                    return std::make_unique<OverlayEntryComponent>(mode, pointer_policy);
                },
                {entry.spec.content}};
            children.push_back(DynamicChildSpec{
                "overlay:" + std::to_string(entry.id), std::move(wrapper)});
        }
        return children;
    }

    void set_structure_invalidator(std::function<void()> invalidator) override {
        state_->structural_invalidator = std::move(invalidator);
    }

    void paint(PaintContext&) const override {}

private:
    std::shared_ptr<OverlayState> state_;
    std::shared_ptr<const Spec> root_;
};

[[nodiscard]] inline Spec make_overlay_host_spec(
    Spec root, const std::shared_ptr<OverlayState>& state) {
    auto retained_root = std::make_shared<const Spec>(std::move(root));
    return Spec{
        [state, retained_root] {
            return std::make_unique<OverlayHostComponent>(state, retained_root);
        },
        {*retained_root}};
}

} // namespace detail

} // namespace ui
