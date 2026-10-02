#include <nativeui/paint.hpp>

#include "detail/gradient_cache_key.hpp"
#include "detail/painter_private_hooks.hpp"

namespace ui {

bool Painter::raster_cache_hook_available() const noexcept {
    return private_hooks_ && private_hooks_->paint_raster_cache_boundary;
}

bool Painter::paint_raster_cache_boundary(
    const detail::RasterCachePaintRequest& request,
    void* callback_state,
    detail::RasterCachePaintCallback paint_callback,
    detail::RasterCacheCommitCallback commit_callback) {
    if (!raster_cache_hook_available()) return false;
    return private_hooks_->paint_raster_cache_boundary(
        private_hooks_->state,
        request,
        canvas_,
        callback_state,
        paint_callback,
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
