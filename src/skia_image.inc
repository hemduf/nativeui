#include <nativeui/image.hpp>
#include <nativeui/paint.hpp>

#include "include/core/SkData.h"
#include "include/core/SkImage.h"
#include "include/core/SkSamplingOptions.h"

#include <memory>
#include <utility>

namespace ui::detail {

struct ImageData {
    sk_sp<SkImage> image;
    Size size{};
};

struct ImageAccess {
    [[nodiscard]] static const std::shared_ptr<const ImageData>& data(const Image& image) noexcept {
        return image.data_;
    }
};

void draw_image(Painter& painter, const Image& image, Rect destination) {
    const auto& data = ImageAccess::data(image);
    if (!data || !data->image || !(destination.w > 0.0f) || !(destination.h > 0.0f)) return;

    const auto dst = SkRect::MakeXYWH(
        destination.x, destination.y, destination.w, destination.h);
    painter.canvas().drawImageRect(
        data->image.get(),
        dst,
        SkSamplingOptions(SkFilterMode::kLinear),
        nullptr);
}

} // namespace ui::detail

namespace ui {

Image Image::decode(std::span<const std::byte> encoded) {
    if (encoded.empty()) return {};

    auto bytes = SkData::MakeWithCopy(encoded.data(), encoded.size());
    if (!bytes) return {};

    auto image = SkImages::DeferredFromEncodedData(std::move(bytes));
    if (!image) return {};

    auto data = std::make_shared<detail::ImageData>();
    data->size = Size{static_cast<float>(image->width()), static_cast<float>(image->height())};
    data->image = std::move(image);
    return Image{std::move(data)};
}

Size Image::size() const noexcept {
    return data_ ? data_->size : Size{};
}

} // namespace ui
