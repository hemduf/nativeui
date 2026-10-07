#include <nativeui/grid_view.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ui::detail {
namespace {
float coordinate(double value) noexcept {
    const double limit = std::numeric_limits<float>::max();
    return static_cast<float>(std::clamp(value, -limit, limit));
}
class GridGeometry final : public VirtualSemanticGeometry {
  public:
    GridGeometry(std::size_t count, std::size_t columns, double width, double height, double gap,
                 Rect viewport, Point offset)
        : count_(count), columns_(columns), width_(width), height_(height), gap_(gap),
          viewport_(viewport), offset_(offset) {}
    [[nodiscard]] std::size_t size() const noexcept override { return count_; }
    [[nodiscard]] Rect bounds_at(std::size_t index) const noexcept override {
        if (index >= count_)
            return {};
        return {coordinate(static_cast<double>(viewport_.x) + (index % columns_) * (width_ + gap_) -
                           offset_.x),
                coordinate(static_cast<double>(viewport_.y) +
                           (index / columns_) * (height_ + gap_) - offset_.y),
                coordinate(width_), coordinate(height_)};
    }

  private:
    std::size_t count_, columns_;
    double width_, height_, gap_;
    Rect viewport_;
    Point offset_;
};
class GridLayout final : public CollectionLayoutPolicy {
  public:
    GridLayout(double width, double height, double gap)
        : minimum_width_(width), height_(height), gap_(gap) {
        if (!std::isfinite(width) || width <= 0 || !std::isfinite(height) || height <= 0 ||
            !std::isfinite(gap) || gap < 0)
            throw std::invalid_argument("GridView sizes must be positive and gap nonnegative");
    }
    [[nodiscard]] CollectionLayoutSnapshot prepare(CollectionHeightSnapshot, std::size_t count,
                                                   Rect viewport, Point offset,
                                                   bool) const override {
        const double fitted = std::max(
            1.0, std::floor((static_cast<double>(viewport.w) + gap_) / (minimum_width_ + gap_)));
        if (fitted >= static_cast<double>(std::numeric_limits<std::size_t>::max()))
            throw std::overflow_error("GridView column count overflow");
        const auto columns = static_cast<std::size_t>(fitted);
        const double width = std::max(0.0, (viewport.w - static_cast<double>(columns - 1) * gap_) /
                                               static_cast<double>(columns));
        const auto rows = count / columns + (count % columns != 0);
        const double total =
            rows ? static_cast<double>(rows) * height_ + static_cast<double>(rows - 1) * gap_ : 0.0;
        if (!std::isfinite(total))
            throw std::overflow_error("GridView total height overflow");
        const double first_value =
            std::floor(std::max(0.0, static_cast<double>(offset.y)) / (height_ + gap_));
        // Clamp in floating point before conversion: a valid tiny cell height
        // can produce a ratio larger than size_t, including an infinite ratio.
        const auto first = first_value >= static_cast<double>(rows)
                               ? rows
                               : static_cast<std::size_t>(first_value);
        const double visible_value =
            std::ceil(std::max(0.0, static_cast<double>(viewport.h)) / (height_ + gap_));
        const auto remaining = rows - first;
        const auto visible_rows = visible_value >= static_cast<double>(remaining)
                                      ? remaining
                                      : static_cast<std::size_t>(visible_value);
        const auto begin = first > 2 ? first - 2 : 0;
        const auto visible_end = first + std::min(rows - first, visible_rows);
        const auto end = visible_end + std::min<std::size_t>(2, rows - visible_end);
        CollectionLayoutSnapshot result;
        result.columns = columns;
        result.content = {viewport.w, coordinate(total)};
        result.geometry = std::make_shared<const GridGeometry>(count, columns, width, height_, gap_,
                                                               viewport, offset);
        for (std::size_t row = begin; row < end; ++row) {
            const auto base = row * columns;
            const auto limit = base + std::min(columns, count - base);
            for (std::size_t index = base; index < limit; ++index)
                result.window.push_back(index);
        }
        return result;
    }
    [[nodiscard]] std::size_t neighbour(std::size_t index, int horizontal, int vertical,
                                        std::size_t columns,
                                        std::size_t count) const noexcept override {
        if (!count)
            return 0;
        const std::size_t distance = vertical ? std::max<std::size_t>(1, columns) : 1;
        if (horizontal < 0 || vertical < 0)
            return index - std::min(index, distance);
        return index + std::min(count - 1 - index, distance);
    }

  private:
    double minimum_width_, height_, gap_;
};
} // namespace
Spec make_grid_view(CollectionSourceFactory source, CollectionViewOptions options,
                    double minimum_width, double cell_height, double gap, bool reorder) {
    options.row_heights = {cell_height, false};
    // The contact engine emits owned keys; application callbacks own mutation.
    options.reorder = reorder;
    return make_collection_view(std::move(source), std::move(options),
                                CollectionHierarchyOptions{false, true, true, 0, 0},
                                std::make_shared<GridLayout>(minimum_width, cell_height, gap));
}
} // namespace ui::detail
