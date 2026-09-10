#pragma once

#include <nativeui/detail/dynamic_source.hpp>
#include <nativeui/detail/virtual_list_window.hpp>
#include <nativeui/layout.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ui::detail {

template <class Key>
class VirtualListRetainedRuntime {
public:
    using Model = VirtualListDatasetModel<Key>;
    using Item = typename Model::Item;
    using RowFactory = std::function<Spec(const Item&)>;

    VirtualListRetainedRuntime(float row_height, RowFactory row_factory)
        : row_height_(row_height),
          window_(model_, row_height_, std::move(row_factory)) {
        scroll_subscription_ = scroll_.observe([this](Point) { refresh_window(); });
    }

    VirtualListRetainedRuntime(const VirtualListRetainedRuntime&) = delete;
    VirtualListRetainedRuntime& operator=(const VirtualListRetainedRuntime&) = delete;

    [[nodiscard]] bool replace(std::vector<Item> items) {
        const auto generation = model_.generation();
        if (!model_.replace(std::move(items))) return false;
        if (model_.generation() != generation) refresh_window();
        return true;
    }

    [[nodiscard]] ScrollState& scroll() noexcept { return scroll_; }
    [[nodiscard]] const ScrollState& scroll() const noexcept { return scroll_; }
    [[nodiscard]] float row_height() const noexcept { return row_height_; }

    [[nodiscard]] bool scroll_to_index(
        std::size_t index,
        ScrollAlignment alignment = ScrollAlignment::Nearest) {
        const auto target = model_.scroll_offset_for_index(
            index,
            row_height_,
            scroll_.viewport_size().h,
            scroll_.offset().y,
            virtual_alignment(alignment));
        if (!target) return false;
        const auto before = scroll_.offset();
        scroll_.set_offset(Point{before.x, *target});
        return true;
    }

    [[nodiscard]] bool scroll_to_key(
        const Key& key,
        ScrollAlignment alignment = ScrollAlignment::Nearest) {
        const auto target = model_.scroll_offset_for_key(
            key,
            row_height_,
            scroll_.viewport_size().h,
            scroll_.offset().y,
            virtual_alignment(alignment));
        if (!target) return false;
        const auto before = scroll_.offset();
        scroll_.set_offset(Point{before.x, *target});
        return true;
    }

    [[nodiscard]] float content_height() const noexcept {
        const auto height = virtual_list_content_height(model_.size(), row_height_);
        return height.value_or(0.0f);
    }

    void set_viewport_height(float height) {
        height = std::isfinite(height) ? std::max(0.0f, height) : 0.0f;
        if (height == viewport_height_) return;
        viewport_height_ = height;
        refresh_window();
    }

    void set_structure_invalidator(std::function<void()> invalidator) {
        structure_invalidator_ = std::move(invalidator);
    }

    [[nodiscard]] std::vector<std::string> desired_keys() const {
        return window_.keys();
    }

    [[nodiscard]] std::vector<DynamicChildSpec> desired_children() const {
        std::vector<DynamicChildSpec> result;
        result.reserve(window_.items().size());
        for (const auto& item : window_.items()) {
            result.push_back(DynamicChildSpec{item.key, *item.payload});
        }
        return result;
    }

    [[nodiscard]] const typename VirtualListMaterializationWindow<Key, Spec>::MaterializedItem*
    materialized_at(std::size_t child_index) const noexcept {
        const auto& items = window_.items();
        return child_index < items.size() ? &items[child_index] : nullptr;
    }

private:
    [[nodiscard]] static VirtualListAlignment virtual_alignment(ScrollAlignment alignment) noexcept {
        switch (alignment) {
        case ScrollAlignment::Nearest: return VirtualListAlignment::Nearest;
        case ScrollAlignment::Start: return VirtualListAlignment::Start;
        case ScrollAlignment::Center: return VirtualListAlignment::Center;
        case ScrollAlignment::End: return VirtualListAlignment::End;
        }
        return VirtualListAlignment::Nearest;
    }

    void refresh_window() {
        const auto previous_keys = window_.keys();
        if (!window_.update(scroll_.offset().y, viewport_height_)) return;
        if (window_.keys() != previous_keys && structure_invalidator_) {
            structure_invalidator_();
        }
    }

    Model model_;
    float row_height_{};
    ScrollState scroll_{ScrollAxis::Vertical};
    VirtualListMaterializationWindow<Key, Spec> window_;
    float viewport_height_{};
    std::function<void()> structure_invalidator_;
    ScrollState::Subscription scroll_subscription_;
};

class VirtualListOwnedSpec final {
public:
    explicit VirtualListOwnedSpec(Spec spec) : spec_(std::move(spec)) {}

    [[nodiscard]] Spec spec() && { return std::move(spec_); }

private:
    Spec spec_;
};

template <class Key>
class VirtualListRetainedComponent final : public Component, public DynamicChildrenSource {
public:
    explicit VirtualListRetainedComponent(std::shared_ptr<VirtualListRetainedRuntime<Key>> runtime)
        : runtime_(std::move(runtime)) {}

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        Size result{0.0f, runtime_->content_height()};
        for (const auto& child : children) result.w = std::max(result.w, child.preferred.w);
        return result;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        Size result{0.0f, runtime_->content_height()};
        for (const auto& child : children) result.w = std::max(result.w, child.minimum.w);
        return result;
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const override {
        // T034's ScrollComponent is the sole viewport/offset authority. By the
        // time this content node is laid out it has already published canonical
        // viewport/content metrics into the shared ScrollState.
        runtime_->set_viewport_height(runtime_->scroll().viewport_size().h);
        const auto row_height = runtime_->row_height();
        for (std::size_t child_index = 0; child_index < placements.size(); ++child_index) {
            const auto* item = runtime_->materialized_at(child_index);
            if (!item) {
                placements[child_index].bounds = {};
                continue;
            }
            const double row_y = static_cast<double>(item->index) * static_cast<double>(row_height);
            placements[child_index].bounds = Rect{
                bounds.x,
                bounds.y + static_cast<float>(row_y),
                bounds.w,
                row_height};
        }
    }

    [[nodiscard]] std::vector<std::string> desired_keys() const override {
        return runtime_->desired_keys();
    }

    [[nodiscard]] std::vector<DynamicChildSpec> desired_children() const override {
        return runtime_->desired_children();
    }

    void set_structure_invalidator(std::function<void()> invalidator) override {
        runtime_->set_structure_invalidator(std::move(invalidator));
    }

    void unmount(LifecycleContext&) override {
        runtime_->set_structure_invalidator({});
    }

    void paint(PaintContext&) const override {}

private:
    std::shared_ptr<VirtualListRetainedRuntime<Key>> runtime_;
};

template <class Key>
[[nodiscard]] Spec make_virtual_list_retained_spec(
    std::shared_ptr<VirtualListRetainedRuntime<Key>> runtime) {
    auto initial_children = runtime->desired_children();
    std::vector<Spec> children;
    children.reserve(initial_children.size());
    for (auto& child : initial_children) children.push_back(std::move(child.spec));

    Spec content{
        [runtime] {
            return std::make_unique<VirtualListRetainedComponent<Key>>(runtime);
        },
        std::move(children)};

    return std::move(ScrollView{
        runtime->scroll(),
        VirtualListOwnedSpec{std::move(content)}}).spec();
}

} // namespace ui::detail
