#pragma once

#include <nativeui/component_tree.hpp>

namespace ui::detail {

// Renderer/retained-integration seam. This is not the public CachedLayer API.
// A current content token is necessary, but is not by itself a GPU cache hit:
// each renderer must also validate its own image, device signature and lifetime.
struct RasterCacheAccess final {
    using Token = RasterCacheEpoch::Token;

    [[nodiscard]] static bool register_boundary(Tree& tree, NodeId id) {
        return tree.register_raster_cache_boundary(id);
    }

    [[nodiscard]] static Token capture(Tree& tree, NodeId id) noexcept {
        return tree.capture_raster_cache_content(id);
    }

    [[nodiscard]] static bool reusable(
        const Tree& tree, NodeId id, const Token& token) noexcept {
        return tree.reusable_raster_cache_content(id, token);
    }

    [[nodiscard]] static bool committable(
        const Tree& tree, NodeId id, const Token& token) noexcept {
        return tree.committable_raster_cache_content(id, token);
    }

    [[nodiscard]] static bool commit(Tree& tree, NodeId id, const Token& token) noexcept {
        return tree.commit_raster_cache_content(id, token);
    }

    [[nodiscard]] static std::function<void()> invalidator(Tree& tree, NodeId id) {
        return tree.raster_cache_invalidator(id);
    }
};

} // namespace ui::detail
