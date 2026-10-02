#pragma once

/// \file
/// Public controller for NativeUI's fixed-row-height virtualized ListView.
///
/// Virtual-list state, dataset replacement, scrolling and semantic projection are
/// UI/main-thread facilities. They may allocate and may invoke the row factory;
/// they are not real-time/audio-thread synchronization APIs.

#include <nativeui/detail/virtual_list_retained.hpp>
#include <nativeui/state.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
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
    /// Internal retained runtime type shared with ListView.
    ///
    /// Normal application code should use VirtualListState rather than mutate
    /// the runtime directly; the alias exists so the controller and ListView can
    /// share one instance-owned retained model without a global registry.
    using Runtime = detail::VirtualListRetainedRuntime<Key>;

    /// Logical dataset row accepted by replace().
    ///
    /// Each Item owns its Key, semantic name/description/actions and flags.
    /// Keys use the same supported domains as dynamic composition
    /// (string-like, integral, or enum) and must be unique in one dataset.
    using Item = typename Runtime::Item;

    /// Shared immutable O(N) semantic metadata for one accepted dataset.
    ///
    /// Copy the shared_ptr value if the metadata must outlive this controller or
    /// a later replace(); holding only the reference returned by
    /// metadata_snapshot() does not extend the controller lifetime.
    using MetadataSnapshot = VirtualSemanticChildren::MetadataSnapshot;

    /// Construct one fixed-row-height virtual-list controller.
    ///
    /// \param selection Borrowed observable selected key. The State must outlive
    /// every controller/view/runtime that still uses this virtual list.
    /// \param row_height Fixed row height in logical UI pixels. Must be finite
    /// and strictly positive; otherwise std::invalid_argument is thrown.
    /// \param row_factory Callable receiving const Item& and returning either a
    /// Spec or any builder accepted by make_spec(). It is moved into the retained
    /// runtime and may be invoked later as rows enter the materialization window.
    /// Captures therefore must outlive those invocations.
    /// \param overscan Extra logical rows materialized around the visible range;
    /// measured in rows, not pixels. The default is 2.
    ///
    /// Construction may allocate. The factory is stored but is not called merely
    /// because a dataset is later supplied to replace(); row creation occurs when
    /// viewport materialization requires it.
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
              overscan)) {
        runtime_->bind_selection(selection);
    }

    /// Controllers are unique owners of their external state handle.
    VirtualListState(const VirtualListState&) = delete;
    /// Controllers are not copy-assignable; ListView shares the retained runtime
    /// explicitly rather than by copying the controller.
    VirtualListState& operator=(const VirtualListState&) = delete;

    /// Replace the complete logical dataset.
    ///
    /// The vector is consumed. Returns false for contract-level rejection such
    /// as duplicate encoded keys, unrepresentable total content height, semantic
    /// token exhaustion, or generation exhaustion; the previously accepted
    /// logical dataset remains valid on those rejection paths.
    ///
    /// An exactly identical dataset is accepted without advancing
    /// dataset_generation(). Stable keys preserve semantic identity across
    /// reorder. Newly visible rows may cause row_factory invocations while the
    /// materialization window refreshes. Allocation or application callback
    /// exceptions are not translated to false and may propagate.
    ///
    /// UI/main-thread only; not suitable for an audio/real-time callback.
    [[nodiscard]] bool replace(std::vector<Item> items) {
        return runtime_->replace(std::move(items));
    }

    /// Scroll so the logical item at \p index satisfies \p alignment.
    ///
    /// Alignment is resolved against the current viewport in logical pixels.
    /// Nearest keeps an already-visible row in place. Returns false when index is
    /// outside the accepted dataset or the requested geometry cannot be
    /// represented. A successful call may materialize rows and therefore invoke
    /// the stored row factory.
    [[nodiscard]] bool scroll_to_index(
        std::size_t index,
        ScrollAlignment alignment = ScrollAlignment::Nearest) {
        return runtime_->scroll_to_index(index, alignment);
    }

    /// Scroll to the item identified by \p key.
    ///
    /// Returns false when the key is absent or scrolling geometry cannot be
    /// represented. Stable-key lookup uses the same encoded identity as
    /// replace(). A successful call may refresh/materialize the viewport.
    [[nodiscard]] bool scroll_to_key(
        const Key& key,
        ScrollAlignment alignment = ScrollAlignment::Nearest) {
        return runtime_->scroll_to_key(key, alignment);
    }

    /// Current scroll offset in logical UI pixels.
    [[nodiscard]] Point offset() const noexcept { return runtime_->scroll().offset(); }
    /// Current laid-out viewport size in logical UI pixels.
    [[nodiscard]] Size viewport_size() const noexcept { return runtime_->scroll().viewport_size(); }
    /// Current logical content extent in logical UI pixels.
    [[nodiscard]] Size content_size() const noexcept { return runtime_->scroll().content_size(); }
    /// Fixed logical row height supplied at construction, in logical UI pixels.
    [[nodiscard]] float row_height() const noexcept { return runtime_->row_height(); }
    /// Configured materialization overscan, measured in logical rows.
    [[nodiscard]] std::size_t overscan() const noexcept { return runtime_->overscan(); }

    /// Monotonic generation of the accepted logical dataset.
    ///
    /// Starts at zero and advances only when replace() publishes a dataset that
    /// differs from the current one. Selection and scrolling do not advance it.
    [[nodiscard]] std::uint64_t dataset_generation() const noexcept {
        return runtime_->dataset_generation();
    }

    /// Borrow the shared immutable semantic metadata handle for this generation.
    ///
    /// The returned reference is valid only while this controller/runtime object
    /// is alive and until ordinary C++ mutation of the owning handle. Copy the
    /// shared_ptr when the metadata itself must remain alive independently.
    /// Reading the metadata never materializes visual rows or calls row_factory.
    [[nodiscard]] const MetadataSnapshot& metadata_snapshot() const noexcept {
        return runtime_->metadata_snapshot();
    }

    /// Capture immutable semantic children for the current dataset/selection.
    ///
    /// \param list_bounds Current ListView bounds in logical UI coordinates.
    /// The returned value shares immutable metadata and derives each row's
    /// logical bounds from row_height(), the current vertical scroll offset and
    /// these bounds. Off-screen semantic queries do not materialize visual rows
    /// and do not invoke row_factory.
    ///
    /// The call reads the bound selection State and is therefore subject to
    /// NativeUI's normal UI/main-thread State contract. Once returned, the
    /// VirtualSemanticChildren value is data-only and keeps its metadata alive.
    [[nodiscard]] VirtualSemanticChildren semantic_children(Rect list_bounds) const {
        return runtime_->semantic_children(list_bounds);
    }

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
