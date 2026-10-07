#include <nativeui/grid.hpp>
#include "detail/layout_support.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ui {
namespace {

float clean_gap(float value) noexcept {
    return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
}

std::size_t checked_end(std::size_t start, std::size_t span) {
    if (span == 0 || start > std::numeric_limits<std::size_t>::max() - span) {
        throw std::invalid_argument("Grid cell has zero span or overflowing endpoint");
    }
    const auto end = start + span;
    if (end > std::vector<Track>{}.max_size()) {
        throw std::invalid_argument("Grid cell exceeds representable track count");
    }
    return end;
}

void validate_cells(const std::vector<std::optional<GridCell>>& cells) {
    for (std::size_t i = 0; i < cells.size(); ++i) {
        if (!cells[i]) continue;
        const auto& a = *cells[i];
        const auto a_row_end = checked_end(a.row, a.row_span);
        const auto a_column_end = checked_end(a.column, a.column_span);
        for (std::size_t j = 0; j < i; ++j) {
            if (!cells[j]) continue;
            const auto& b = *cells[j];
            if (a.row < b.row + b.row_span && b.row < a_row_end &&
                a.column < b.column + b.column_span && b.column < a_column_end) {
                throw std::invalid_argument("Grid explicit cells overlap");
            }
        }
    }
}

float checked_extent(double value) {
    if (!std::isfinite(value) || value > std::numeric_limits<float>::max()) {
        throw std::invalid_argument("Grid extent is not representable in logical coordinates");
    }
    return static_cast<float>(std::max(0.0, value));
}

float checked_coordinate(double value) {
    if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max()) {
        throw std::invalid_argument("Grid coordinate is not representable");
    }
    return static_cast<float>(value);
}

void validate_track_extent(const std::vector<Track>& tracks, std::size_t count, float gap) {
    double extent = count > 1 ? static_cast<double>(gap) * static_cast<double>(count - 1) : 0.0;
    for (const auto& track : tracks) {
        if (track.type() == TrackType::Fixed) extent += track.value();
    }
    (void)checked_extent(extent);
}

struct CellMap {
    std::vector<std::optional<GridCell>> cells;
    std::size_t columns{};
    std::size_t rows{};
};

CellMap place_cells(std::size_t column_count, std::size_t row_count,
                    const std::vector<std::optional<GridCell>>& explicit_cells,
                    const std::vector<ChildMetrics>& children) {
    CellMap result{std::vector<std::optional<GridCell>>(children.size()), column_count, row_count};
    std::vector<std::size_t> reservations;
    for (std::size_t i = 0; i < children.size(); ++i) {
        if (!children[i].participates_in_layout || i >= explicit_cells.size() ||
            !explicit_cells[i]) continue;
        result.cells[i] = explicit_cells[i];
        reservations.push_back(i);
        result.columns = std::max(result.columns,
            checked_end(explicit_cells[i]->column, explicit_cells[i]->column_span));
        result.rows = std::max(result.rows,
            checked_end(explicit_cells[i]->row, explicit_cells[i]->row_span));
    }

    std::size_t row = 0;
    std::size_t column = 0;
    for (std::size_t i = 0; i < children.size(); ++i) {
        if (!children[i].participates_in_layout || result.cells[i]) continue;
        // Jump to the end of an occupied rectangle, avoiding an allocation or
        // scan per cell when an explicit span covers a large region.
        bool searched_from_row_start = column == 0;
        auto first_reservation_end = std::numeric_limits<std::size_t>::max();
        for (;;) {
            bool occupied = false;
            for (auto j : reservations) {
                const auto& reserved = *explicit_cells[j];
                if (row >= reserved.row && row < reserved.row + reserved.row_span &&
                    column >= reserved.column && column < reserved.column + reserved.column_span) {
                    column = reserved.column + reserved.column_span;
                    first_reservation_end = std::min(
                        first_reservation_end, reserved.row + reserved.row_span);
                    occupied = true;
                    break;
                }
            }
            if (column >= result.columns) {
                column = 0;
                // When the complete row is covered by explicit rectangles,
                // every row until the first rectangle ends is covered too.
                row = searched_from_row_start ? first_reservation_end : checked_end(row, 1);
                searched_from_row_start = true;
                first_reservation_end = std::numeric_limits<std::size_t>::max();
            }
            if (!occupied) break;
        }
        result.cells[i] = GridCell{row, column, 1, 1};
        result.rows = std::max(result.rows, checked_end(row, 1));
        ++column;
        if (column == result.columns) {
            column = 0;
            row = checked_end(row, 1);
        }
    }
    return result;
}

std::vector<detail::GridTrackMetrics> track_metrics(
    const std::vector<Track>& tracks, const CellMap& map,
    const std::vector<ChildMetrics>& children, float gap, bool horizontal) {
    std::vector<detail::GridTrackMetrics> result(tracks.size());
    std::vector<std::size_t> order;
    order.reserve(children.size());
    for (std::size_t i = 0; i < children.size(); ++i) {
        if (map.cells[i]) order.push_back(i);
    }
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        const auto& left = *map.cells[a];
        const auto& right = *map.cells[b];
        return (horizontal ? left.column_span : left.row_span) <
               (horizontal ? right.column_span : right.row_span);
    });
    for (auto i : order) {
        const auto& cell = *map.cells[i];
        const auto begin = horizontal ? cell.column : cell.row;
        const auto span = horizontal ? cell.column_span : cell.row_span;
        const auto minimum = horizontal ? children[i].minimum.w : children[i].minimum.h;
        const auto preferred = horizontal ? children[i].preferred.w : children[i].preferred.h;
        for (bool use_minimum : {true, false}) {
            double current = static_cast<double>(gap) * static_cast<double>(span - 1);
            std::size_t flexible_count = 0;
            for (std::size_t j = begin; j < begin + span; ++j) {
                if (tracks[j].type() == TrackType::Fixed) {
                    current += tracks[j].value();
                } else {
                    current += use_minimum ? result[j].minimum :
                        std::max(result[j].minimum, result[j].preferred);
                    ++flexible_count;
                }
            }
            const double target = use_minimum ? std::max(0.0f, minimum) :
                std::max(std::max(0.0f, minimum), preferred);
            if (target <= current || flexible_count == 0) continue;
            const auto extra = (target - current) / static_cast<double>(flexible_count);
            for (std::size_t j = begin; j < begin + span; ++j) {
                if (tracks[j].type() == TrackType::Fixed) continue;
                if (use_minimum) {
                    result[j].minimum = checked_extent(result[j].minimum + extra);
                } else {
                    result[j].preferred = checked_extent(
                        std::max(result[j].minimum, result[j].preferred) + extra);
                }
            }
        }
    }
    return result;
}

float natural_extent(const std::vector<Track>& tracks,
                     const std::vector<detail::GridTrackMetrics>& content,
                     float gap, bool minimum) {
    double result = tracks.size() > 1 ?
        static_cast<double>(gap) * static_cast<double>(tracks.size() - 1) : 0.0;
    for (std::size_t i = 0; i < tracks.size(); ++i) {
        result += tracks[i].type() == TrackType::Fixed ? tracks[i].value() :
            (minimum ? content[i].minimum : std::max(content[i].minimum, content[i].preferred));
    }
    return checked_extent(result);
}

float fixed_span_constraint(const std::vector<Track>& tracks, std::size_t start,
                            std::size_t span, float gap, float fallback) {
    double result = static_cast<double>(gap) * static_cast<double>(span - 1);
    for (std::size_t i = start; i < start + span; ++i) {
        if (i >= tracks.size() || tracks[i].type() != TrackType::Fixed) return fallback;
        result += tracks[i].value();
    }
    return checked_extent(result);
}

std::vector<float> track_positions(const std::vector<float>& sizes, float start, float gap) {
    std::vector<float> positions(sizes.size(), start);
    double position = start;
    for (std::size_t i = 0; i < sizes.size(); ++i) {
        positions[i] = checked_coordinate(position);
        position += sizes[i];
        if (i + 1 < sizes.size()) position += gap;
    }
    (void)checked_coordinate(position);
    return positions;
}

float span_extent(const std::vector<float>& sizes, std::size_t start,
                  std::size_t span, float gap) {
    double result = static_cast<double>(gap) * static_cast<double>(span - 1);
    for (std::size_t i = start; i < start + span; ++i) result += sizes[i];
    return checked_extent(result);
}

} // namespace

Track Track::fixed(float extent) noexcept { return Track{TrackType::Fixed, sanitize(extent)}; }
Track Track::auto_size() noexcept { return Track{TrackType::Auto, 0.0f}; }
Track Track::flex(float weight) noexcept { return Track{TrackType::Flex, sanitize(weight)}; }
TrackType Track::type() const noexcept { return type_; }
float Track::value() const noexcept { return value_; }
Track::Track(TrackType type, float value) : type_(type), value_(value) {}
float Track::sanitize(float value) noexcept { return clean_gap(value); }

struct GridComponent::Layout {
    CellMap map;
    std::vector<Track> columns;
    std::vector<Track> rows;
    std::vector<detail::GridTrackMetrics> column_content;
    std::vector<detail::GridTrackMetrics> row_content;
};

GridComponent::GridComponent(GridTracks tracks, float column_gap, float row_gap)
    : GridComponent(std::move(tracks), column_gap, row_gap, {}) {}

GridComponent::GridComponent(GridTracks tracks, float column_gap, float row_gap,
                             std::vector<std::optional<GridCell>> cells)
    : columns_(std::move(tracks.columns)), rows_(std::move(tracks.rows)),
      cells_(std::move(cells)), column_gap_(clean_gap(column_gap)), row_gap_(clean_gap(row_gap)) {
    if (columns_.empty()) columns_.push_back(Track::auto_size());
    if (rows_.empty()) rows_.push_back(Track::auto_size());
    validate_cells(cells_);
    validate_track_extent(columns_, columns_.size(), column_gap_);
    validate_track_extent(rows_, rows_.size(), row_gap_);
}

GridComponent::Layout GridComponent::prepare_layout(const std::vector<ChildMetrics>& children) const {
    auto map = place_cells(columns_.size(), rows_.size(), cells_, children);
    validate_track_extent(columns_, map.columns, column_gap_);
    validate_track_extent(rows_, map.rows, row_gap_);
    auto columns = columns_;
    auto rows = rows_;
    columns.resize(map.columns, Track::auto_size());
    rows.resize(map.rows, Track::auto_size());
    auto column_content = track_metrics(columns, map, children, column_gap_, true);
    auto row_content = track_metrics(rows, map, children, row_gap_, false);
    // Reject overflowing combined content before publishing any placements.
    (void)natural_extent(columns, column_content, column_gap_, false);
    (void)natural_extent(rows, row_content, row_gap_, false);
    return Layout{std::move(map), std::move(columns), std::move(rows),
                  std::move(column_content), std::move(row_content)};
}

Size GridComponent::measure(const std::vector<ChildMetrics>& children) const {
    const auto layout = prepare_layout(children);
    return {natural_extent(layout.columns, layout.column_content, column_gap_, false),
            natural_extent(layout.rows, layout.row_content, row_gap_, false)};
}

Size GridComponent::minimum_size(const std::vector<ChildMetrics>& children) const {
    const auto layout = prepare_layout(children);
    return {natural_extent(layout.columns, layout.column_content, column_gap_, true),
            natural_extent(layout.rows, layout.row_content, row_gap_, true)};
}

Constraints GridComponent::child_constraints(const Constraints& constraints,
                                             std::size_t index, std::size_t count) const {
    return child_constraints(constraints, index, std::vector<ChildMetrics>(count));
}

Constraints GridComponent::child_constraints(const Constraints& constraints, std::size_t index,
                                             const std::vector<ChildMetrics>& metadata) const {
    const auto map = place_cells(columns_.size(), rows_.size(), cells_, metadata);
    if (index >= map.cells.size() || !map.cells[index]) return constraints.loosen();
    const auto& cell = *map.cells[index];
    return Constraints::loose({
        fixed_span_constraint(columns_, cell.column, cell.column_span, column_gap_, constraints.max.w),
        fixed_span_constraint(rows_, cell.row, cell.row_span, row_gap_, constraints.max.h)});
}

void GridComponent::layout_children(Rect bounds, const std::vector<ChildMetrics>& children,
                                    std::vector<ChildPlacement>& placements) const {
    const auto layout = prepare_layout(children);
    const auto column_sizes = detail::allocate_grid_tracks(
        layout.columns, layout.column_content, std::max(0.0f, bounds.w), column_gap_);
    const auto row_sizes = detail::allocate_grid_tracks(
        layout.rows, layout.row_content, std::max(0.0f, bounds.h), row_gap_);
    const auto column_positions = track_positions(column_sizes, bounds.x, column_gap_);
    const auto row_positions = track_positions(row_sizes, bounds.y, row_gap_);
    std::vector<ChildPlacement> candidate(children.size());
    for (std::size_t i = 0; i < children.size(); ++i) {
        if (!layout.map.cells[i]) {
            candidate[i].bounds = {bounds.x, bounds.y, 0.0f, 0.0f};
            continue;
        }
        const auto& cell = *layout.map.cells[i];
        candidate[i].bounds = {
            column_positions[cell.column], row_positions[cell.row],
            span_extent(column_sizes, cell.column, cell.column_span, column_gap_),
            span_extent(row_sizes, cell.row, cell.row_span, row_gap_)};
    }
    placements = std::move(candidate);
}

void GridComponent::paint(PaintContext&) const {}

void Grid::append_cell(GridCell cell, Spec child) {
    // Reserve both buffers first so an allocation failure cannot misalign the
    // owned children and their optional positions on a surviving builder.
    children_.reserve(children_.size() + 1);
    cells_.reserve(cells_.size() + 1);
    children_.push_back(std::move(child));
    cells_.push_back(cell);
}

Grid&& Grid::gap(float value) && { column_gap_ = value; row_gap_ = value; return std::move(*this); }
Grid&& Grid::column_gap(float value) && { column_gap_ = value; return std::move(*this); }
Grid&& Grid::row_gap(float value) && { row_gap_ = value; return std::move(*this); }

Spec Grid::spec() && {
    // Validate structural claims before consuming anything from the builder,
    // including cells that might subsequently be hidden by application state.
    validate_cells(cells_);
    const auto map = place_cells(std::max<std::size_t>(1, tracks_.columns.size()),
                                std::max<std::size_t>(1, tracks_.rows.size()), cells_,
                                std::vector<ChildMetrics>(children_.size()));
    validate_track_extent(tracks_.columns, map.columns, clean_gap(column_gap_));
    validate_track_extent(tracks_.rows, map.rows, clean_gap(row_gap_));
    auto tracks = std::move(tracks_);
    auto cells = std::move(cells_);
    const float column_gap = column_gap_;
    const float row_gap = row_gap_;
    return Spec{[tracks = std::move(tracks), cells = std::move(cells), column_gap, row_gap] {
                    return std::make_unique<GridComponent>(tracks, column_gap, row_gap, cells);
                }, std::move(children_)};
}

} // namespace ui
