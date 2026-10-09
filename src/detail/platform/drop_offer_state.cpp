#include "drop_offer_state.hpp"
#include "../view_geometry.hpp"

#include <algorithm>
#include <cstdint>

namespace ui::detail {

bool ViewDropOffer::accept(PuglView* view, std::string_view type,
                           Rect logical_region, float scale) {
    if (!view || !active_offer_ || active_offer_->clipboard != PUGL_CLIPBOARD_DRAG) {
        return false;
    }
    const auto count = puglGetNumClipboardTypes(view, PUGL_CLIPBOARD_DRAG);
    for (std::uint32_t i = 0; i < count; ++i) {
        const char* offered = puglGetClipboardType(view, PUGL_CLIPBOARD_DRAG, i);
        if (!offered || type != offered) continue;

        const auto physical = logical_to_physical_covering_rect(logical_region, scale);
        const auto status = puglAcceptOffer(
            view, active_offer_, i, PUGL_DATA_ACTION_COPY,
            static_cast<int>(physical.x), static_cast<int>(physical.y),
            static_cast<unsigned>(std::max(1.0f, physical.w)),
            static_cast<unsigned>(std::max(1.0f, physical.h)));
        decided_ = status == PUGL_SUCCESS;
        return decided_;
    }
    return false;
}

void ViewDropOffer::reject(PuglView* view, Rect logical_region, float scale) {
    if (!view || !active_offer_ || active_offer_->clipboard != PUGL_CLIPBOARD_DRAG) {
        return;
    }
    const auto physical = logical_to_physical_covering_rect(logical_region, scale);
    (void)puglRejectOffer(
        view, active_offer_,
        static_cast<int>(physical.x), static_cast<int>(physical.y),
        static_cast<unsigned>(std::max(1.0f, physical.w)),
        static_cast<unsigned>(std::max(1.0f, physical.h)));
    decided_ = true;
}

} // namespace ui::detail
