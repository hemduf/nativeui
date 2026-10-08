#pragma once

#include <nativeui/component.hpp>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace ui {

/// Track sizing mode: fixed logical extent, content-dependent auto size or
/// dimensionless flexible share of available space.
enum class TrackType { Fixed, Auto, Flex };

/// Value describing one grid row/column sizing policy.
/// Invalid or negative requested extents/weights are normalized by factories.
class Track {
public:
/// Use a fixed extent in logical UI units.
    [[nodiscard]] static Track fixed(float extent) noexcept;
/// Size from the track's measured children.
    [[nodiscard]] static Track auto_size() noexcept;
/// Assign a dimensionless share of remaining space.
    [[nodiscard]] static Track flex(float weight = 1.0f) noexcept;
    [[nodiscard]] TrackType type() const noexcept;
    [[nodiscard]] float value() const noexcept;

private:
    Track(TrackType type, float value);
    [[nodiscard]] static float sanitize(float value) noexcept;
    TrackType type_{TrackType::Auto};
    float value_{};
};

/// Owned row and column sizing vectors for a retained Grid.
struct GridTracks {
    std::vector<Track> columns;
    std::vector<Track> rows;
};

/// Zero-based explicit cell. Constructor children occupy the next free cell in
/// row-major order after active explicit cells have reserved their rectangles.
struct GridCell {
    std::size_t row{};
    std::size_t column{};
    std::size_t row_span{1};
    std::size_t column_span{1};
};

/// Retained grid measurement and placement using explicit or automatic cells.
class GridComponent final : public Component {
public:
    GridComponent(GridTracks tracks, float column_gap, float row_gap);
    GridComponent(GridTracks tracks, float column_gap, float row_gap,
                  std::vector<std::optional<GridCell>> cells);

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override;
    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override;
    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t index, std::size_t count) const override;
    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t index,
        const std::vector<ChildMetrics>& metadata) const override;
    void layout_children(Rect bounds, const std::vector<ChildMetrics>& children,
                         std::vector<ChildPlacement>& placements) const override;
    void paint(PaintContext&) const override;

private:
    struct Layout;
    [[nodiscard]] Layout prepare_layout(const std::vector<ChildMetrics>& children) const;
    std::vector<Track> columns_;
    std::vector<Track> rows_;
    std::vector<std::optional<GridCell>> cells_;
    float column_gap_{};
    float row_gap_{};
};

/// Detached grid builder; owns Track vectors and child recipes.
/// Constructor children receive automatic row-major positions; cell()
/// appends an explicitly positioned child.
class Grid {
public:
    template <class... Children>
/// Take ownership of track definitions and convert all children to Specs.
    Grid(GridTracks tracks, Children&&... children) : tracks_(std::move(tracks)) {
        (children_.push_back(make_spec(std::forward<Children>(children))), ...);
        cells_.resize(children_.size());
    }

    template <class Child>
/// Append a child with explicit zero-based GridCell position and spans.
/// Invalid placements are handled by retained grid layout validation.
    Grid&& cell(GridCell cell, Child&& child) && {
        append_cell(cell, make_spec(std::forward<Child>(child)));
        return std::move(*this);
    }

/// Set both horizontal and vertical gap in logical pixels.
    Grid&& gap(float value) &&;
/// Set horizontal gap independently.
    Grid&& column_gap(float value) &&;
/// Set vertical gap independently.
    Grid&& row_gap(float value) &&;
/// Consume tracks, cells and child Specs into retained layout.
    Spec spec() &&;

private:
    void append_cell(GridCell cell, Spec child);
    GridTracks tracks_;
    float column_gap_{};
    float row_gap_{};
    std::vector<Spec> children_;
    std::vector<std::optional<GridCell>> cells_;
};

} // namespace ui
