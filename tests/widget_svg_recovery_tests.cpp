#include <nativeui/paint.hpp>
#include <nativeui/svg.hpp>
#include "include/core/SkSurface.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace ui::detail {
struct PainterSvgFaultAccess {
    static void fail_after_transform(Painter& painter) noexcept {
        painter.layer_fault_point_ = Painter::LayerFaultPoint::AfterSvgTransform;
    }
};
} // namespace ui::detail

int main() {
    try {
        constexpr std::string_view source =
            R"(<svg xmlns="http://www.w3.org/2000/svg" width="4" height="2"><rect width="4" height="2" fill="#ff0000"/></svg>)";
        const auto icon = ui::SvgIcon::parse({reinterpret_cast<const std::byte*>(source.data()), source.size()});
        auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(32, 24));
        if (!icon.valid() || !surface) throw std::runtime_error("fixture invalid");
        auto& canvas = *surface->getCanvas();
        canvas.clear(SK_ColorBLACK);
        canvas.translate(2, 3);
        const auto before_matrix = canvas.getTotalMatrix();
        const int before_count = canvas.getSaveCount();
        ui::Painter painter{canvas};
        bool caught = false;
        try {
            const auto outer = painter.scoped_clip({0, 0, 28, 20});
            ui::detail::PainterSvgFaultAccess::fail_after_transform(painter);
            ui::detail::draw_svg(painter, icon, {4, 4, 16, 12});
        } catch (const std::bad_alloc&) { caught = true; }
        if (!caught) throw std::runtime_error("fault did not fire");
        if (canvas.getSaveCount() != before_count)
            throw std::runtime_error("SVG fault leaked backend save stack");
        if (canvas.getTotalMatrix() != before_matrix || painter.save_depth() != 0)
            throw std::runtime_error("SVG fault changed parent transform");
        ui::detail::draw_svg(painter, icon, {0, 0, 8, 4});
        painter.fill_rounded_rect({10, 0, 4, 4}, 0, {0, 0, 1, 1});
        SkPixmap pixels;
        if (!surface->peekPixels(&pixels)) throw std::runtime_error("pixels unavailable");
        if (pixels.getColor(3, 4) != SK_ColorRED || pixels.getColor(13, 4) != SK_ColorBLUE)
            throw std::runtime_error("next SVG or neighboring paint did not recover");
        if (canvas.getSaveCount() != before_count || canvas.getTotalMatrix() != before_matrix)
            throw std::runtime_error("successful SVG changed caller state");
        std::cout << "PASS widget_svg_recovery\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL widget_svg_recovery: " << error.what() << '\n';
        return 1;
    }
}
