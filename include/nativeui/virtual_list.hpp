#pragma once

#include <nativeui/detail/virtual_list_retained.hpp>
#include <nativeui/state.hpp>

#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace ui {

template <class Key>
class ListView;

/// External state/controller for the fixed-row-height virtualized ListView path.
///
/// The controller owns one retained virtual-list runtime, keeps the logical
/// selection outside O(N) semantic metadata and remains valid after the ListView
/// builder has been consumed into a UI tree. This mirrors ScrollState's explicit
/// ownership model and makes imperative scroll_to_index/key operations possible
/// without a global registry or hidden current-list handle.
template <class Key>
class VirtualListState {
public:
    using Runtime = detail::VirtualListRetainedRuntime<Key>;
    using Item = typename Runtime::Item;

    template <class RowFactory>
    VirtualListState(
        State<std::optional<Key>>& selection,
        float row_height,
        RowFactory&& row_factory,
        std::size_t overscan = 2)
        : selection_(&selection),
          runtime_(std::make_shared<Runtime>(
              checked_row_height(row_height),
              adapt_row_factory(std::forward<RowFactory>(row_factory)),
              overscan)) {}

    VirtualListState(const VirtualListState&) = delete;
    VirtualListState& operator=(const VirtualListState&) = delete;

    [[nodiscard]] bool replace(std::vector<Item> items) {
        return runtime_->replace(std::move(items));
    }

    [[nodiscard]] bool scroll_to_index(
        std::size_t index,
        ScrollAlignment alignment = ScrollAlignment::Nearest) {
        return runtime_->scroll_to_index(index, alignment);
    }

    [[nodiscard]] bool scroll_to_key(
        const Key& key,
        ScrollAlignment alignment = ScrollAlignment::Nearest) {
        return runtime_->scroll_to_key(key, alignment);
    }

    [[nodiscard]] Point offset() const noexcept { return runtime_->scroll().offset(); }
    [[nodiscard]] Size viewport_size() const noexcept { return runtime_->scroll().viewport_size(); }
    [[nodiscard]] Size content_size() const noexcept { return runtime_->scroll().content_size(); }
    [[nodiscard]] float row_height() const noexcept { return runtime_->row_height(); }
    [[nodiscard]] std::size_t overscan() const noexcept { return runtime_->overscan(); }

private:
    template <class>
    friend class ListView;

    template <class RowFactory>
    [[nodiscard]] static typename Runtime::RowFactory adapt_row_factory(RowFactory&& row_factory) {
        using Factory = std::decay_t<RowFactory>;
        using Result = std::invoke_result_t<Factory&, const Item&>;

        return [factory = Factory(std::forward<RowFactory>(row_factory))](const Item& item) mutable
                   -> Spec {
            if constexpr (std::is_same_v<std::remove_cvref_t<Result>, Spec>) {
                return std::invoke(factory, item);
            } else {
                return make_spec(std::invoke(factory, item));
            }
        };
    }

    [[nodiscard]] static float checked_row_height(float value) {
        if (!std::isfinite(value) || !(value > 0.0f)) {
            throw std::invalid_argument("VirtualListState row_height must be finite and positive");
        }
        return value;
    }

    [[nodiscard]] State<std::optional<Key>>& selection() const noexcept { return *selection_; }
    [[nodiscard]] const std::shared_ptr<Runtime>& runtime() const noexcept { return runtime_; }

    State<std::optional<Key>>* selection_{};
    std::shared_ptr<Runtime> runtime_;
};

} // namespace ui
