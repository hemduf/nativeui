#include <nativeui/component_tree.hpp>
#include <nativeui/paint.hpp>

#include "detail/gradient_cache_key.hpp"
#include "detail/painter_private_hooks.hpp"

namespace ui {

bool Tree::try_paint_raster_cache_boundary(
    const Node& node,
    Painter& painter,
    PlatformServices& platform,
    Rect inherited_clip) {
    const auto boundary = raster_cache_epochs_.find(node.id);
    if (boundary == raster_cache_epochs_.end() ||
        !painter.private_hooks_ ||
        !painter.private_hooks_->paint_raster_cache_boundary ||
        raster_cache_has_nested_boundary(node)) {
        return false;
    }

    // Until nested raster-space/compositing rules are qualified, bypass every
    // boundary participating in nesting, including an inner boundary.
    for (const Node* ancestor = node.parent; ancestor; ancestor = ancestor->parent) {
        if (raster_cache_epochs_.find(ancestor->id) != raster_cache_epochs_.end()) {
            return false;
        }
    }

    const auto subtree = raster_cache_subtree_visual_bounds(node);
    if (!subtree) return false;
    const Rect visible = intersect(*subtree, inherited_clip);
    if (visible.empty()) return true;

    const Rect local_extent{
        visible.x - node.bounds.x,
        visible.y - node.bounds.y,
        visible.w,
        visible.h};
    auto token = capture_raster_cache_content(node.id);
    if (token.expired()) return false;

    struct CallbackState final {
        Tree* tree{};
        const Node* node{};
        PlatformServices* platform{};
        Rect inherited_clip{};
        Painter* parent_painter{};
        detail::RasterCacheEpoch::Token token{};
        bool reuse_safe{true};
    } state{
        this, &node, &platform, inherited_clip, &painter, token, true};

    const detail::RasterCachePaintRequest request{
        node.id,
        token,
        local_extent,
        visible,
        reusable_raster_cache_content(node.id, token)};

    const auto paint_callback =
        [](void* raw,
           SkCanvas& canvas,
           const detail::PainterPrivateHooks* hooks) -> bool {
            auto& callback = *static_cast<CallbackState*>(raw);
            Painter nested{canvas, hooks};
            callback.tree->paint_node_contents(
                *callback.node,
                nested,
                *callback.platform,
                callback.inherited_clip);
            callback.parent_painter->used_effects_ |= nested.used_effects();
            callback.reuse_safe &= !nested.used_effects();
            return true;
        };

    const auto validate_callback = [](void* raw) noexcept -> bool {
        const auto& callback = *static_cast<CallbackState*>(raw);
        return callback.reuse_safe &&
               callback.tree->committable_raster_cache_content(
                   callback.node->id, callback.token);
    };

    const auto commit_callback = [](void* raw) noexcept -> bool {
        auto& callback = *static_cast<CallbackState*>(raw);
        return callback.reuse_safe &&
               callback.tree->commit_raster_cache_content(
                   callback.node->id, callback.token);
    };

    return painter.private_hooks_->paint_raster_cache_boundary(
        painter.private_hooks_->state,
        request,
        painter.canvas_,
        &state,
        paint_callback,
        validate_callback,
        commit_callback);
}

void Painter::apply_fill_source(
    SkPaint& paint,
    const LinearGradient& gradient) {
    const auto& stops = gradient.stops();
    if (stops.empty()) {
        paint.setColor4f(to_sk_color(Color{}));
        return;
    }
    if (!valid_gradient_stops(stops)) {
        paint.setColor4f(to_sk_color(stops.front().color));
        return;
    }

    auto shader =
        private_hooks_ && private_hooks_->materialize_linear_gradient
        ? private_hooks_->materialize_linear_gradient(
              private_hooks_->state, gradient)
        : detail::GradientCacheAccess::materialize(gradient);
    if (shader) {
        paint.setShader(std::move(shader));
    } else {
        paint.setColor4f(to_sk_color(stops.front().color));
    }
}

void Painter::apply_fill_source(
    SkPaint& paint,
    const RadialGradient& gradient) {
    const auto& stops = gradient.stops();
    if (stops.empty()) {
        paint.setColor4f(to_sk_color(Color{}));
        return;
    }
    if (!valid_gradient_stops(stops)) {
        paint.setColor4f(to_sk_color(stops.front().color));
        return;
    }

    auto shader =
        private_hooks_ && private_hooks_->materialize_radial_gradient
        ? private_hooks_->materialize_radial_gradient(
              private_hooks_->state, gradient)
        : detail::GradientCacheAccess::materialize(gradient);
    if (shader) {
        paint.setShader(std::move(shader));
    } else {
        paint.setColor4f(to_sk_color(stops.front().color));
    }
}

sk_sp<SkImageFilter> Painter::materialize_effect_filter(
    const Effect& effect) {
    if (private_hooks_ && private_hooks_->materialize_effect_filter) {
        return private_hooks_->materialize_effect_filter(
            private_hooks_->state, effect);
    }
    switch (effect.kind_) {
        case Effect::Kind::GaussianBlur:
            return SkImageFilters::Blur(
                effect.sigma_x_, effect.sigma_y_, SkTileMode::kDecal, nullptr);
        case Effect::Kind::DropShadow:
            return SkImageFilters::DropShadow(
                effect.offset_.x,
                effect.offset_.y,
                effect.sigma_x_,
                effect.sigma_y_,
                to_sk_color(effect.color_),
                nullptr,
                nullptr);
        case Effect::Kind::DropShadowOnly:
            return SkImageFilters::DropShadowOnly(
                effect.offset_.x,
                effect.offset_.y,
                effect.sigma_x_,
                effect.sigma_y_,
                to_sk_color(effect.color_),
                nullptr,
                nullptr);
    }
    return nullptr;
}

void Painter::apply_fill_source(
    SkPaint& paint,
    const ImageTexture& texture) {
    auto shader =
        private_hooks_ && private_hooks_->materialize_image_texture
        ? private_hooks_->materialize_image_texture(
              private_hooks_->state, texture)
        : detail::materialize_image_texture(texture);
    if (!shader) {
        throw std::runtime_error(
            "NativeUI image texture materialization returned no shader");
    }
    paint.setShader(std::move(shader));
}

void Painter::apply_fill_source(
    SkPaint& paint,
    const std::shared_ptr<const detail::ShaderBrushSnapshot>& snapshot) {
    auto shader =
        private_hooks_ && private_hooks_->materialize_shader_brush
        ? private_hooks_->materialize_shader_brush(
              private_hooks_->state, snapshot)
        : detail::materialize_shader_brush(snapshot);
    if (!shader) {
        throw std::runtime_error(
            "NativeUI runtime shader materialization returned no shader");
    }
    paint.setShader(std::move(shader));
}

} // namespace ui
