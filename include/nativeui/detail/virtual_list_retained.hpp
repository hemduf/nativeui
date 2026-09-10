#pragma once

#include <nativeui/detail/dynamic_source.hpp>
#include <nativeui/detail/virtual_list_window.hpp>
#include <nativeui/layout.hpp>
#include <nativeui/state.hpp>

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
    using ActivationCallback = std::function<void(const Key&)>;

    VirtualListRetainedRuntime(
        float row_height,
        RowFactory row_factory,
        std::size_t overscan = 2)
        : row_height_(row_height),
          overscan_(overscan),
          window_(model_, row_height_, std::move(row_factory)) {
        scroll_subscription_ = scroll_.observe([this](Point) { refresh_window(); });
    }

    VirtualListRetainedRuntime(const VirtualListRetainedRuntime&) = delete;
    VirtualListRetainedRuntime& operator=(const VirtualListRetainedRuntime&) = delete;

    void bind_selection(State<std::optional<Key>>& selection) noexcept {
        selection_ = &selection;
    }

    [[nodiscard]] State<std::optional<Key>>* selection_state() const noexcept {
        return selection_;
    }

    void set_activation_callback(ActivationCallback callback) {
        activation_callback_ = std::move(callback);
    }

    [[nodiscard]] bool replace(std::vector<Item> items) {
        // Reject geometry that cannot be represented before publishing the
        // logical dataset/semantic generation. A rejected update therefore
        // leaves the previous dataset, metadata and materialized rows intact.
        if (!virtual_list_content_height(items.size(), row_height_)) return false;

        std::optional<Key> focused_key;
        if (focused_index_) {
            if (const auto* item = model_.item_at(*focused_index_)) focused_key = item->key;
        }
        std::optional<Key> captured_key;
        if (captured_index_) {
            if (const auto* item = model_.item_at(*captured_index_)) captured_key = item->key;
        }

        const auto generation = model_.generation();
        if (!model_.replace(std::move(items))) return false;
        if (model_.generation() == generation) return true;

        // Dataset changes are the one place where O(N) logical-key lookup is
        // allowed. Ordinary scrolling carries scalar indices so it never scans
        // the full dataset merely to preserve focused/captured exception rows.
        focused_index_ = focused_key ? model_.index_of_key(*focused_key) : std::nullopt;
        captured_index_ = captured_key ? model_.index_of_key(*captured_key) : std::nullopt;
        refresh_window();
        return true;
    }

    [[nodiscard]] ScrollState& scroll() noexcept { return scroll_; }
    [[nodiscard]] const ScrollState& scroll() const noexcept { return scroll_; }
    [[nodiscard]] float row_height() const noexcept { return row_height_; }
    [[nodiscard]] std::size_t overscan() const noexcept { return overscan_; }
    [[nodiscard]] std::size_t size() const noexcept { return model_.size(); }

    [[nodiscard]] bool enabled_at(std::size_t index) const noexcept {
        const auto* item = model_.item_at(index);
        return item && item->enabled;
    }

    [[nodiscard]] std::optional<std::size_t> selected_index() const {
        if (!selection_ || !selection_->get()) return std::nullopt;
        const auto index = model_.index_of_key(*selection_->get());
        return index && enabled_at(*index) ? index : std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> first_enabled() const noexcept {
        for (std::size_t index = 0; index < model_.size(); ++index) {
            if (enabled_at(index)) return index;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> last_enabled() const noexcept {
        for (std::size_t index = model_.size(); index > 0; --index) {
            if (enabled_at(index - 1)) return index - 1;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> next_enabled(std::size_t from) const noexcept {
        for (std::size_t index = from + 1; index < model_.size(); ++index) {
            if (enabled_at(index)) return index;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> previous_enabled(std::size_t from) const noexcept {
        for (std::size_t index = from; index > 0; --index) {
            if (enabled_at(index - 1)) return index - 1;
        }
        return std::nullopt;
    }

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

    [[nodiscard]] bool select(std::size_t index) {
        if (!selection_ || !enabled_at(index)) return false;
        const auto* item = model_.item_at(index);
        if (!item) return false;

        const Key key = item->key;
        (void)scroll_to_index(index, ScrollAlignment::Nearest);
        auto* const selection = selection_;
        selection->set(std::optional<Key>{key});
        return true;
    }

    [[nodiscard]] bool activate(std::size_t index) {
        if (!enabled_at(index)) return false;
        const auto* item = model_.item_at(index);
        if (!item) return false;
        const Key key = item->key;
        auto callback = activation_callback_;
        if (!select(index)) return false;
        if (callback) callback(key);
        return true;
    }

    void reveal_selection() {
        if (!selection_ || !selection_->get()) return;
        if (const auto index = model_.index_of_key(*selection_->get())) {
            (void)scroll_to_index(*index, ScrollAlignment::Nearest);
        }
    }

    void set_focused_index(std::optional<std::size_t> index) {
        if (index && *index >= model_.size()) index.reset();
        if (focused_index_ == index) return;
        focused_index_ = index;
        refresh_window();
    }

    void set_captured_index(std::optional<std::size_t> index) {
        if (index && *index >= model_.size()) index.reset();
        if (captured_index_ == index) return;
        captured_index_ = index;
        refresh_window();
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
        if (!window_.update(
                scroll_.offset().y,
                viewport_height_,
                overscan_,
                focused_index_,
                captured_index_)) {
            return;
        }
        if (window_.keys() != previous_keys && structure_invalidator_) {
            structure_invalidator_();
        }
    }

    Model model_;
    float row_height_{};
    std::size_t overscan_{2};
    ScrollState scroll_{ScrollAxis::Vertical};
    VirtualListMaterializationWindow<Key, Spec> window_;
    float viewport_height_{};
    State<std::optional<Key>>* selection_{};
    ActivationCallback activation_callback_;
    std::optional<std::size_t> focused_index_;
    std::optional<std::size_t> captured_index_;
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

    // T036's rows stay outside global focus traversal even though their visual
    // subtrees are retained dynamically by T058.
    [[nodiscard]] bool is_focus_scope() const noexcept override { return true; }
    [[nodiscard]] bool focus_scope_active() const noexcept override { return false; }
    [[nodiscard]] bool focus_scope_traps() const noexcept override { return false; }

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
class VirtualListViewComponent final : public Component {
public:
    explicit VirtualListViewComponent(std::shared_ptr<VirtualListRetainedRuntime<Key>> runtime)
        : runtime_(std::move(runtime)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

    void mount(MountContext& context) override {
        if (auto* selection = selection_state()) {
            auto runtime = runtime_;
            auto invalidate = context.invalidator();
            selection_subscription_ = selection->observe(
                [runtime = std::move(runtime), invalidate = std::move(invalidate)](
                    const std::optional<Key>&) {
                    runtime->reveal_selection();
                    invalidate();
                });
        }
    }

    void unmount(LifecycleContext&) override { selection_subscription_.reset(); }

    void focus_changed(bool focused, FocusContext&) override {
        if (!focused) {
            active_index_.reset();
            runtime_->set_focused_index(std::nullopt);
            return;
        }
        active_index_ = runtime_->selected_index();
        if (!active_index_) active_index_ = runtime_->first_enabled();
        runtime_->set_focused_index(active_index_);
    }

    EventResult input(const InputEvent& event, InputContext&) override {
        if (event.type != InputType::KeyDown) return EventResult::Ignored;

        if (const auto selected = runtime_->selected_index()) {
            active_index_ = selected;
        } else if (!active_index_) {
            active_index_ = runtime_->first_enabled();
        }

        if (event.key == Key::Enter || event.key == Key::Space) {
            if (!active_index_) return EventResult::Ignored;
            return runtime_->activate(*active_index_)
                ? EventResult::Handled
                : EventResult::Ignored;
        }

        std::optional<std::size_t> target;
        switch (event.key) {
        case Key::Down:
            target = active_index_
                ? runtime_->next_enabled(*active_index_)
                : runtime_->first_enabled();
            break;
        case Key::Up:
            target = active_index_
                ? runtime_->previous_enabled(*active_index_)
                : runtime_->last_enabled();
            break;
        case Key::Home:
            target = runtime_->first_enabled();
            break;
        case Key::End:
            target = runtime_->last_enabled();
            break;
        default:
            return EventResult::Ignored;
        }

        if (target) {
            active_index_ = target;
            runtime_->set_focused_index(target);
            (void)runtime_->select(*target);
        }
        return EventResult::Handled;
    }

    void paint(PaintContext&) const override {}

private:
    [[nodiscard]] State<std::optional<Key>>* selection_state() const noexcept {
        // The public controller always binds selection. The internal retained
        // runtime tests intentionally leave it null because they exercise only
        // materialization/scroll ownership.
        return runtime_->selection_state();
    }

    std::shared_ptr<VirtualListRetainedRuntime<Key>> runtime_;
    std::optional<std::size_t> active_index_;
    typename State<std::optional<Key>>::Subscription selection_subscription_;
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

    auto scroll = std::move(ScrollView{
        runtime->scroll(),
        VirtualListOwnedSpec{std::move(content)}}).spec();

    std::vector<Spec> root_children;
    root_children.push_back(std::move(scroll));
    return Spec{
        [runtime] {
            return std::make_unique<VirtualListViewComponent<Key>>(runtime);
        },
        std::move(root_children)};
}

} // namespace ui::detail
