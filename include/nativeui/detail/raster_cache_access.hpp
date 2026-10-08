#pragma once

#include <nativeui/component_tree.hpp>

namespace ui::detail {

// Renderer/retained-integration seam. This is not the public CachedLayer API.
// A current content token is necessary, but is not by itself a GPU cache hit:
// each renderer must also validate its own image, device signature and lifetime.
struct RasterCacheAccess final {
    using Token = RasterCacheEpoch::Token;

    [[nodiscard]] static bool register_root(Tree& tree) {
        return tree.root_ &&
               tree.register_raster_cache_boundary(tree.root_->id);
    }

    [[nodiscard]] static bool invalidate_root(Tree& tree) {
        if (!tree.root_) return false;
        const auto found = tree.raster_cache_epochs_.find(tree.root_->id);
        if (found == tree.raster_cache_epochs_.end()) return false;
        tree.invalidate_raster_cache_ancestry(tree.root_.get());
        tree.mark_paint_culling_dirty();
        if (tree.layout_transaction_active_ ||
            !tree.root_->visual_bounds_published) {
            tree.invalidate_paint(tree.viewport_rect());
        } else {
            tree.publish_subtree_visual_bounds_then_invalidate(*tree.root_);
        }
        return true;
    }

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
