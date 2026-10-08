#pragma once

#include <nativeui/geometry.hpp>
#include <pugl/pugl.h>

#include <string_view>

namespace ui::detail {

/// One view's temporarily borrowed Pugl drop offer. Borrow lifetime and
/// reentrant restoration are controlled by ScopedBorrowState at dispatch.
class ViewDropOffer final {
public:
    [[nodiscard]] const PuglDataOfferEvent*& offer_slot() noexcept {
        return active_offer_;
    }

    [[nodiscard]] bool& decision_slot() noexcept { return decided_; }
    [[nodiscard]] bool offer_decided() const noexcept { return decided_; }

    [[nodiscard]] bool accept(PuglView* view, std::string_view type,
                              Rect logical_region, float scale);
    void reject(PuglView* view, Rect logical_region, float scale);

private:
    const PuglDataOfferEvent* active_offer_{};
    bool decided_{};
};

} // namespace ui::detail
