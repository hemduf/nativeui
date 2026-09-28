#pragma once

#include <nativeui/image.hpp>

#include <memory>

namespace ui::detail {

struct ImageAccess {
    [[nodiscard]] static const std::shared_ptr<const ImageData>& data(
        const Image& image) noexcept {
        return image.data_;
    }

    [[nodiscard]] static const ImageData* identity(const Image& image) noexcept {
        return image.data_.get();
    }
};

} // namespace ui::detail
