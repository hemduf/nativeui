
class RowComponent final : public Component {
public:
    RowComponent(float gap, Align align = Align::Start, Justify justify = Justify::Start)
        : gap_(std::isfinite(gap) ? std::max(0.0f, gap) : 0.0f),
          align_(align),
          justify_(justify) {}

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        Size result{};
        for (std::size_t i = 0; i < children.size(); ++i) {
            result.w += children[i].preferred.w;
            if (i) result.w += gap_;
            result.h = std::max(result.h, children[i].preferred.h);
        }
        return result;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        Size result{};
        for (std::size_t i = 0; i < children.size(); ++i) {
            result.w += children[i].minimum.w;
            if (i) result.w += gap_;
            result.h = std::max(result.h, children[i].minimum.h);
        }
        return result;
    }

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const override {
        // Row measures intrinsic widths without prematurely constraining the main
        // axis; flex allocation happens during placement. Height stays bounded.
        return Constraints::loose({kUnboundedExtent, constraints.max.h});
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>& children,
        std::vector<ChildPlacement>& placements) const override {
        const float available_w = std::max(0.0f, bounds.w);
        const float available_h = std::max(0.0f, bounds.h);

        const auto allocation = detail::allocate_main_axis(
            children, available_w, gap_, [](const Size size) { return size.w; });
        const float actual_gap =
            detail::distributed_gap(justify_, gap_, allocation.free_space, children.size());
        float x = bounds.x + detail::main_axis_offset(justify_, allocation.free_space);

        for (std::size_t i = 0; i < children.size(); ++i) {
            const auto size = children[i].preferred;
            const float child_w = allocation.extents[i];
            const float natural_h = std::min(std::max(0.0f, size.h), available_h);
            const float child_h = align_ == Align::Stretch ? available_h : natural_h;
            const float child_y =
                bounds.y + detail::cross_axis_offset(align_, available_h, child_h);
            placements[i].bounds = Rect{x, child_y, child_w, child_h};
            x += child_w + actual_gap;
        }
    }

    void paint(PaintContext&) const override {}

private:
    float gap_{};
    Align align_{Align::Start};
    Justify justify_{Justify::Start};
};


class ColumnComponent final : public Component {
public:
    ColumnComponent(
        float gap,
        float padding,
        Align align = Align::Start,
        Justify justify = Justify::Start)
        : gap_(std::isfinite(gap) ? std::max(0.0f, gap) : 0.0f),
          padding_(std::isfinite(padding) ? std::max(0.0f, padding) : 0.0f),
          align_(align),
          justify_(justify) {}

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        Size result{padding_ * 2.0f, padding_ * 2.0f};
        for (std::size_t i = 0; i < children.size(); ++i) {
            result.w = std::max(result.w, children[i].preferred.w + padding_ * 2.0f);
            result.h += children[i].preferred.h;
            if (i) result.h += gap_;
        }
        return result;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        Size result{padding_ * 2.0f, padding_ * 2.0f};
        for (std::size_t i = 0; i < children.size(); ++i) {
            result.w = std::max(result.w, children[i].minimum.w + padding_ * 2.0f);
            result.h += children[i].minimum.h;
            if (i) result.h += gap_;
        }
        return result;
    }

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const override {
        const auto inner = constraints.inset(padding_, padding_);
        // Column constrains child width but allows natural vertical measurement.
        return Constraints::loose({inner.max.w, kUnboundedExtent});
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>& children,
        std::vector<ChildPlacement>& placements) const override {
        const float inner_x = bounds.x + padding_;
        const float inner_y = bounds.y + padding_;
        const float available_w = std::max(0.0f, bounds.w - padding_ * 2.0f);
        const float available_h = std::max(0.0f, bounds.h - padding_ * 2.0f);

        const auto allocation = detail::allocate_main_axis(
            children, available_h, gap_, [](const Size size) { return size.h; });
        const float actual_gap =
            detail::distributed_gap(justify_, gap_, allocation.free_space, children.size());
        float y = inner_y + detail::main_axis_offset(justify_, allocation.free_space);

        for (std::size_t i = 0; i < children.size(); ++i) {
            const auto size = children[i].preferred;
            const float child_h = allocation.extents[i];
            const float natural_w = std::min(std::max(0.0f, size.w), available_w);
            const float child_w = align_ == Align::Stretch ? available_w : natural_w;
            const float child_x =
                inner_x + detail::cross_axis_offset(align_, available_w, child_w);
            placements[i].bounds = Rect{child_x, y, child_w, child_h};
            y += child_h + actual_gap;
        }
    }

    void paint(PaintContext&) const override {}

private:
    float gap_{};
    float padding_{};
    Align align_{Align::Start};
    Justify justify_{Justify::Start};
};



class ScrollComponent final : public Component {
public:
    explicit ScrollComponent(ScrollState& state) : state_(&state) {}

    [[nodiscard]] bool clips_children() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        if (children.empty()) return {};
        auto minimum = children.front().minimum;
        if (state_->axis() == ScrollAxis::Horizontal || state_->axis() == ScrollAxis::Both) {
            minimum.w = 0.0f;
        }
        if (state_->axis() == ScrollAxis::Vertical || state_->axis() == ScrollAxis::Both) {
            minimum.h = 0.0f;
        }
        return minimum;
    }

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const override {
        Size maximum = constraints.max;
        if (state_->axis() == ScrollAxis::Horizontal || state_->axis() == ScrollAxis::Both) {
            maximum.w = kUnboundedExtent;
        }
        if (state_->axis() == ScrollAxis::Vertical || state_->axis() == ScrollAxis::Both) {
            maximum.h = kUnboundedExtent;
        }
        return Constraints::loose(maximum);
    }

    void mount(MountContext& context) override {
        subscription_ = state_->observe(
            [invalidate = context.layout_invalidator()](Point) { invalidate(); });
    }

    void unmount(LifecycleContext&) override { subscription_.reset(); }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>& children,
        std::vector<ChildPlacement>& placements) const override {
        if (placements.empty() || children.empty()) {
            state_->update_metrics({bounds.w, bounds.h}, {});
            return;
        }

        Size content = children.front().preferred;
        if (state_->axis() == ScrollAxis::Vertical) content.w = std::max(content.w, bounds.w);
        if (state_->axis() == ScrollAxis::Horizontal) content.h = std::max(content.h, bounds.h);
        state_->update_metrics({bounds.w, bounds.h}, content);
        const auto offset = state_->offset();
        placements.front().bounds = Rect{
            bounds.x - offset.x, bounds.y - offset.y, content.w, content.h};
    }

    void paint(PaintContext&) const override {}

private:
    ScrollState* state_{};
    ScrollState::Subscription subscription_;
};

class ClipComponent final : public Component {
public:
    [[nodiscard]] bool clips_children() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }

    [[nodiscard]] Constraints child_constraints(
        const Constraints&, std::size_t, std::size_t) const override {
        // A clip is a viewport, not a sizing container: measure its content at
        // natural size so the descendant can intentionally overflow the clip.
        return Constraints::unbounded();
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>& children,
        std::vector<ChildPlacement>& placements) const override {
        if (placements.empty() || children.empty()) return;
        const auto size = children.front().preferred;
        placements.front().bounds = Rect{
            bounds.x, bounds.y, std::max(0.0f, size.w), std::max(0.0f, size.h)};
    }

    void paint(PaintContext&) const override {}
};


class GridComponent final : public Component {
public:
    GridComponent(GridTracks tracks, float column_gap, float row_gap)
        : columns_(std::move(tracks.columns)),
          rows_(std::move(tracks.rows)),
          column_gap_(sanitize_gap(column_gap)),
          row_gap_(sanitize_gap(row_gap)) {
        if (columns_.empty()) columns_.push_back(Track::auto_size());
        if (rows_.empty()) rows_.push_back(Track::auto_size());
    }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        const auto columns = column_metrics(children);
        const auto rows = row_metrics(children);
        return Size{natural_extent(columns_, columns, column_gap_, false),
                    natural_extent(effective_rows(children.size()), rows, row_gap_, false)};
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        const auto columns = column_metrics(children);
        const auto rows = row_metrics(children);
        return Size{natural_extent(columns_, columns, column_gap_, true),
                    natural_extent(effective_rows(children.size()), rows, row_gap_, true)};
    }

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t index, std::size_t count) const override {
        const std::size_t column_count = columns_.size();
        const std::size_t column = index % column_count;
        const std::size_t row = index / column_count;
        const auto rows = effective_rows(count);

        float max_w = constraints.max.w;
        float max_h = constraints.max.h;
        if (columns_[column].type() == TrackType::Fixed) max_w = columns_[column].value();
        if (row < rows.size() && rows[row].type() == TrackType::Fixed) max_h = rows[row].value();
        return Constraints::loose({max_w, max_h});
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>& children,
        std::vector<ChildPlacement>& placements) const override {
        const auto rows = effective_rows(children.size());
        const auto column_content = column_metrics(children);
        const auto row_content = row_metrics(children);
        const auto column_sizes = detail::allocate_grid_tracks(
            columns_, column_content, std::max(0.0f, bounds.w), column_gap_);
        const auto row_sizes = detail::allocate_grid_tracks(
            rows, row_content, std::max(0.0f, bounds.h), row_gap_);

        std::vector<float> column_positions(column_sizes.size(), bounds.x);
        for (std::size_t i = 1; i < column_positions.size(); ++i) {
            column_positions[i] = column_positions[i - 1] + column_sizes[i - 1] + column_gap_;
        }
        std::vector<float> row_positions(row_sizes.size(), bounds.y);
        for (std::size_t i = 1; i < row_positions.size(); ++i) {
            row_positions[i] = row_positions[i - 1] + row_sizes[i - 1] + row_gap_;
        }

        const std::size_t column_count = columns_.size();
        for (std::size_t i = 0; i < children.size(); ++i) {
            const std::size_t column = i % column_count;
            const std::size_t row = i / column_count;
            if (row >= row_sizes.size()) break;
            placements[i].bounds = Rect{
                column_positions[column],
                row_positions[row],
                column_sizes[column],
                row_sizes[row]};
        }
    }

    void paint(PaintContext&) const override {}

private:
    [[nodiscard]] static float sanitize_gap(float value) noexcept {
        return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
    }

    [[nodiscard]] std::vector<Track> effective_rows(std::size_t child_count) const {
        auto rows = rows_;
        const std::size_t required =
            (child_count + columns_.size() - 1) / columns_.size();
        while (rows.size() < required) rows.push_back(Track::auto_size());
        return rows;
    }

    [[nodiscard]] std::vector<detail::GridTrackMetrics> column_metrics(
        const std::vector<ChildMetrics>& children) const {
        std::vector<detail::GridTrackMetrics> result(columns_.size());
        for (std::size_t i = 0; i < children.size(); ++i) {
            auto& track = result[i % columns_.size()];
            track.minimum = std::max(track.minimum, children[i].minimum.w);
            track.preferred = std::max(track.preferred, children[i].preferred.w);
        }
        return result;
    }

    [[nodiscard]] std::vector<detail::GridTrackMetrics> row_metrics(
        const std::vector<ChildMetrics>& children) const {
        const auto rows = effective_rows(children.size());
        std::vector<detail::GridTrackMetrics> result(rows.size());
        for (std::size_t i = 0; i < children.size(); ++i) {
            auto& track = result[i / columns_.size()];
            track.minimum = std::max(track.minimum, children[i].minimum.h);
            track.preferred = std::max(track.preferred, children[i].preferred.h);
        }
        return result;
    }

    [[nodiscard]] static float natural_extent(
        const std::vector<Track>& tracks,
        const std::vector<detail::GridTrackMetrics>& content,
        float gap,
        bool minimum) {
        float result = tracks.size() > 1
                           ? sanitize_gap(gap) * static_cast<float>(tracks.size() - 1)
                           : 0.0f;
        for (std::size_t i = 0; i < tracks.size(); ++i) {
            const auto metrics = i < content.size() ? content[i] : detail::GridTrackMetrics{};
            if (tracks[i].type() == TrackType::Fixed) {
                result += tracks[i].value();
            } else {
                result += minimum ? metrics.minimum : std::max(metrics.minimum, metrics.preferred);
            }
        }
        return result;
    }

    std::vector<Track> columns_;
    std::vector<Track> rows_;
    float column_gap_{};
    float row_gap_{};
};


class FlexComponent final : public Component {
public:
    FlexComponent(float grow, float shrink)
        : factors_{detail::flex_weight(grow), detail::flex_weight(shrink)} {}

    [[nodiscard]] FlexFactors flex_factors() const noexcept override { return factors_; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds = bounds;
    }

    void paint(PaintContext&) const override {}

private:
    FlexFactors factors_{};
};


class SpacerComponent final : public Component {
public:
    explicit SpacerComponent(Size size)
        : size_{std::isfinite(size.w) ? std::max(0.0f, size.w) : 0.0f,
                std::isfinite(size.h) ? std::max(0.0f, size.h) : 0.0f} {}

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>&) const override {
        return size_;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>&) const override {
        return size_;
    }

    void paint(PaintContext&) const override {}

private:
    Size size_{};
};


class StackComponent final : public Component {
public:
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        Size result{};
        for (const auto& child : children) {
            result.w = std::max(result.w, child.preferred.w);
            result.h = std::max(result.h, child.preferred.h);
        }
        return result;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        Size result{};
        for (const auto& child : children) {
            result.w = std::max(result.w, child.minimum.w);
            result.h = std::max(result.h, child.minimum.h);
        }
        return result;
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const override {
        for (auto& placement : placements) placement.bounds = bounds;
    }

    void paint(PaintContext&) const override {}
};


class PaddingComponent final : public Component {
public:
    explicit PaddingComponent(float padding) : padding_(std::max(0.0f, padding)) {}

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        if (children.empty()) return Size{padding_ * 2.0f, padding_ * 2.0f};
        return Size{children.front().preferred.w + padding_ * 2.0f,
                    children.front().preferred.h + padding_ * 2.0f};
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        if (children.empty()) return Size{padding_ * 2.0f, padding_ * 2.0f};
        return Size{children.front().minimum.w + padding_ * 2.0f,
                    children.front().minimum.h + padding_ * 2.0f};
    }

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t, std::size_t) const override {
        return constraints.inset(padding_, padding_).loosen();
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const override {
        if (placements.empty()) return;
        placements.front().bounds = Rect{bounds.x + padding_,
                                         bounds.y + padding_,
                                         std::max(0.0f, bounds.w - 2.0f * padding_),
                                         std::max(0.0f, bounds.h - 2.0f * padding_)};
    }

    void paint(PaintContext&) const override {}

private:
    float padding_{};
};



