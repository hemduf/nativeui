#include <nativeui/image.hpp>
#include <nativeui/paint.hpp>
#include <nativeui/svg.hpp>

#include "include/core/SkCanvas.h"
#include "include/core/SkStream.h"
#include "modules/svg/include/SkSVGDOM.h"

#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

namespace ui::detail {

struct SvgData {
    sk_sp<SkSVGDOM> dom;
    Size intrinsic{};
};

struct SvgAccess {
    [[nodiscard]] static const std::shared_ptr<const SvgData>& data(const SvgIcon& icon) noexcept {
        return icon.data_;
    }
};

namespace {

[[nodiscard]] bool drawable_rect(Rect rect) noexcept {
    return std::isfinite(rect.x) && std::isfinite(rect.y) &&
           std::isfinite(rect.w) && std::isfinite(rect.h) &&
           rect.w > 0.0f && rect.h > 0.0f;
}

[[nodiscard]] bool drawable_size(Size size) noexcept {
    return std::isfinite(size.w) && std::isfinite(size.h) &&
           size.w > 0.0f && size.h > 0.0f;
}

[[nodiscard]] Size resolved_intrinsic_size(SkSVGDOM& dom) noexcept {
    const auto sk_size = dom.containerSize();
    Size intrinsic{sk_size.width(), sk_size.height()};
    if (drawable_size(intrinsic)) return intrinsic;

    const auto* root = dom.getRoot();
    if (!root) return {};

    const auto& view_box = root->getViewBox();
    if (!view_box || view_box->isEmpty() ||
        !std::isfinite(view_box->width()) || !std::isfinite(view_box->height()) ||
        view_box->width() <= 0.0f || view_box->height() <= 0.0f) {
        return {};
    }

    const float aspect = view_box->width() / view_box->height();
    const bool width_valid = std::isfinite(intrinsic.w) && intrinsic.w > 0.0f;
    const bool height_valid = std::isfinite(intrinsic.h) && intrinsic.h > 0.0f;

    if (width_valid && !height_valid) {
        intrinsic.h = intrinsic.w / aspect;
    } else if (!width_valid && height_valid) {
        intrinsic.w = intrinsic.h * aspect;
    } else {
        intrinsic = Size{view_box->width(), view_box->height()};
    }

    if (!drawable_size(intrinsic)) return {};

    // Root SVGs with omitted/percentage dimensions need a concrete viewport.
    // Set it once at parse time so the cached DOM remains immutable while it is
    // reused from paint paths at arbitrary destination sizes.
    dom.setContainerSize(SkSize::Make(intrinsic.w, intrinsic.h));
    return intrinsic;
}

} // namespace

void draw_svg(Painter& painter, const SvgIcon& icon, Rect destination) {
    const auto& data = SvgAccess::data(icon);
    if (!data || !data->dom || !drawable_size(data->intrinsic) || !drawable_rect(destination)) {
        return;
    }

    auto& canvas = painter.canvas();
    canvas.save();
    canvas.clipRect(SkRect::MakeXYWH(
        destination.x, destination.y, destination.w, destination.h));
    canvas.translate(destination.x, destination.y);
    canvas.scale(destination.w / data->intrinsic.w,
                 destination.h / data->intrinsic.h);
    data->dom->render(&canvas);
    canvas.restore();
}

} // namespace ui::detail

namespace ui {

SvgIcon SvgIcon::parse(std::span<const std::byte> encoded) {
    if (encoded.empty()) return {};

    SkMemoryStream stream(encoded.data(), encoded.size(), true);
    auto dom = SkSVGDOM::MakeFromStream(stream);
    if (!dom) return {};

    const Size intrinsic = detail::resolved_intrinsic_size(*dom);
    if (!detail::drawable_size(intrinsic)) return {};

    auto data = std::make_shared<detail::SvgData>();
    data->dom = std::move(dom);
    data->intrinsic = intrinsic;
    return SvgIcon{std::move(data)};
}

Size SvgIcon::intrinsic_size() const noexcept {
    return data_ ? data_->intrinsic : Size{};
}

struct SvgCache::Impl {
    explicit Impl(ResourceProvider& provider_in)
        : provider(&provider_in) {}

    ResourceProvider* provider{};
    std::unordered_map<std::string, SvgLoadResult> entries;
};

SvgCache::SvgCache(ResourceProvider& provider)
    : impl_(std::make_unique<Impl>(provider)) {}

SvgCache::~SvgCache() = default;
SvgCache::SvgCache(SvgCache&&) noexcept = default;
SvgCache& SvgCache::operator=(SvgCache&&) noexcept = default;

SvgLoadResult SvgCache::load(std::string_view resource_id) {
    if (!impl_ || !impl_->provider) {
        return {{}, SvgLoadError::NotFound};
    }

    const std::string key{resource_id};
    if (const auto existing = impl_->entries.find(key); existing != impl_->entries.end()) {
        return existing->second;
    }

    SvgLoadResult result;
    const auto encoded = impl_->provider->load(resource_id);
    if (!encoded) {
        result.error = SvgLoadError::NotFound;
    } else {
        result.icon = SvgIcon::parse(*encoded);
        if (!result.icon.valid()) {
            result.error = SvgLoadError::ParseFailed;
        }
    }

    impl_->entries.emplace(key, result);
    return result;
}

std::size_t SvgCache::size() const noexcept {
    return impl_ ? impl_->entries.size() : 0U;
}

void SvgCache::clear() {
    if (impl_) impl_->entries.clear();
}

} // namespace ui
