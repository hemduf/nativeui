#include <nativeui/paint.hpp>
#include <nativeui/svg.hpp>

#include "include/core/SkCanvas.h"
#include "include/core/SkStream.h"
#include "modules/svg/include/SkSVGDOM.h"

#include <cmath>
#include <memory>

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

    const auto sk_size = dom->containerSize();
    const Size intrinsic{sk_size.width(), sk_size.height()};
    if (!std::isfinite(intrinsic.w) || !std::isfinite(intrinsic.h) ||
        intrinsic.w <= 0.0f || intrinsic.h <= 0.0f) {
        return {};
    }

    auto data = std::make_shared<detail::SvgData>();
    data->dom = std::move(dom);
    data->intrinsic = intrinsic;
    return SvgIcon{std::move(data)};
}

Size SvgIcon::intrinsic_size() const noexcept {
    return data_ ? data_->intrinsic : Size{};
}

} // namespace ui
