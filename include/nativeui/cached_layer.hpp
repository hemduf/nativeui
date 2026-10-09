#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/cached_layer_source.hpp>

#include <algorithm>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace ui {

/// An explicit raster memoization boundary around one retained subtree.
/// Layout, component identity, input, focus and semantics remain those of the
/// child. The owning renderer may reuse the subtree's pixels while its content
/// is unchanged, or paint normally when caching is unsupported.
///
/// Dependencies supplement normal descendant invalidation: omitting a State
/// never suppresses a paint/content change already known to the retained tree.
/// Construction, State updates and retained use belong on the UI thread.
template <class Child>
class CachedLayer final {
public:
    explicit CachedLayer(Child&& child)
        : child_(make_spec(std::forward<Child>(child))) {}

    /// Observe heterogeneous State sources once per mounted layer. Equal writes
    /// remain no-ops under State's contract; repeated sources are deduplicated.
    /// Handles retain source control blocks, not State objects, so destruction
    /// before mount or before unmount is safe. Repeated calls add dependencies.
    template <detail::StateValue... T>
    CachedLayer&& depends(State<T>&... sources) && {
        auto prepared = dependencies_;
        (append_dependency(prepared, sources), ...);
        dependencies_.swap(prepared);
        return std::move(*this);
    }

    /// Compile-time annotation preserves the child's original component and
    /// key. Copying the resulting Spec creates independent mounted caches.
    [[nodiscard]] Spec spec() && {
        auto source = std::make_shared<const detail::CachedLayerSource>(
            detail::CachedLayerSource{dependencies_, child_.cached_layer});
        child_.cached_layer = std::move(source);
        return std::move(child_);
    }

private:
    template <detail::StateValue T>
    static void append_dependency(
        std::vector<detail::CachedLayerDependency>& destination, State<T>& state) {
        auto source = state.binding();
        const auto identity = detail::StateDependencyAccess::identity(source);
        const auto duplicate = std::find_if(destination.begin(), destination.end(),
            [identity](const auto& existing) { return existing.identity() == identity; });
        if (duplicate == destination.end()) destination.emplace_back(std::move(source));
    }

    Spec child_;
    std::vector<detail::CachedLayerDependency> dependencies_;
};

template <class Child>
CachedLayer(Child&&) -> CachedLayer<Child>;

// Preserve a distinct outer declaration instead of selecting the implicit
// copy deduction candidate for directly nested CachedLayer expressions.
template <class Child>
CachedLayer(CachedLayer<Child>&&) -> CachedLayer<CachedLayer<Child>>;

} // namespace ui
