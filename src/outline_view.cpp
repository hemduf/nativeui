#include <nativeui/outline_view.hpp>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace ui::detail {
namespace {
class OutlineLayout final : public CollectionLayoutPolicy {
  public:
    [[nodiscard]] CollectionLayoutSnapshot prepare(CollectionHeightSnapshot heights,
                                                   std::size_t count, Rect viewport, Point offset,
                                                   bool virtualized) const override {
        if (count != heights.size())
            throw std::logic_error("OutlineView height/row mismatch");
        const double total = heights.total_height();
        if (!std::isfinite(total))
            throw std::overflow_error("OutlineView height overflow");
        CollectionLayoutSnapshot result;
        result.content = {viewport.w,
                          static_cast<float>(std::min(
                              total, static_cast<double>(std::numeric_limits<float>::max())))};
        const auto range =
            virtualized ? heights.range(offset.y, viewport.h, 2) : CollectionRange{0, count};
        result.window.reserve(range.last - range.first);
        for (std::size_t index = range.first; index < range.last; ++index)
            result.window.push_back(index);
        result.geometry = std::make_shared<const CollectionVerticalGeometry>(std::move(heights),
                                                                             viewport, offset);
        return result;
    }
};
} // namespace
std::shared_ptr<CollectionLayoutPolicy> make_outline_view_layout() {
    return std::make_shared<OutlineLayout>();
}
Spec make_outline_view(CollectionSourceFactory source, CollectionViewOptions options,
                       double indentation, double chevron_width) {
    return make_collection_view(
        std::move(source), std::move(options),
        CollectionHierarchyOptions{true, true, false, indentation, chevron_width},
        make_outline_view_layout());
}
} // namespace ui::detail
