/// Detached builder for the retained scrolling content transform.
///
/// It owns the converted child recipe and borrows ScrollState. Prefer ScrollView
/// when standard wheel/pan routing and scrollbar overlays are wanted.
class Scroll {
public:
    /// Borrow state and own the converted child Spec.
    template <class Child>
    Scroll(ScrollState& state, Child&& child)
        : state_(&state), axis_(state.axis()), lifetime_(state.lifetime_token()) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    /// Consume into a retained recipe that keeps only a weak state borrow.
    ///
    /// Materialization may allocate/throw; the returned Spec does not borrow this
    /// builder object.
    Spec spec() && {
        auto* state = state_;
        const auto axis = axis_;
        const auto lifetime = lifetime_;
        return Spec{
            [state, axis, lifetime] {
                return std::make_unique<detail::RetainedScrollComponent>(
                    state, axis, lifetime);
            },
            std::move(children_)};
    }

private:
    ScrollState* state_{};
    ScrollAxis axis_{ScrollAxis::Vertical};
    ScrollState::LifetimeToken lifetime_;
    std::vector<Spec> children_;
};

/// Detached one-child clipping builder.
///
/// Clip owns the child recipe and clips retained painting/hit-testing to its
/// laid-out bounds.
class Clip {
public:
    /// Own the child after make_spec() conversion.
    template <class Child>
    explicit Clip(Child&& child) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    /// Consume into a retained clipping recipe.
    Spec spec() && {
        return Spec{[] { return std::make_unique<ClipComponent>(); }, std::move(children_)};
    }

private:
    std::vector<Spec> children_;
};


/// Detached grid-layout builder.
///
/// Grid consumes owned track lists and child recipes. Track/gap geometry is in
/// logical UI pixels; flex weights are dimensionless.
class Grid {
public:
    /// Consume track lists and own each converted child recipe.
    template <class... Children>
    Grid(GridTracks tracks, Children&&... children) : tracks_(std::move(tracks)) {
        (children_.push_back(make_spec(std::forward<Children>(children))), ...);
    }

    /// Set both row/column gaps in logical pixels.
    ///
    /// Retained layout normalizes negative/non-finite values to zero.
    Grid&& gap(float value) && {
        column_gap_ = value;
        row_gap_ = value;
        return std::move(*this);
    }

    /// Set logical-pixel column gap; invalid values normalize on materialization.
    Grid&& column_gap(float value) && {
        column_gap_ = value;
        return std::move(*this);
    }

    /// Set logical-pixel row gap; invalid values normalize on materialization.
    Grid&& row_gap(float value) && {
        row_gap_ = value;
        return std::move(*this);
    }

    Spec spec() && {
        auto tracks = std::move(tracks_);
        const float column_gap = column_gap_;
        const float row_gap = row_gap_;
        return Spec{
            [tracks = std::move(tracks), column_gap, row_gap] {
                return std::make_unique<GridComponent>(tracks, column_gap, row_gap);
            },
            std::move(children_)};
    }

private:
    GridTracks tracks_;
    float column_gap_{};
    float row_gap_{};
    std::vector<Spec> children_;
};


/// Detached one-child flex-weight wrapper.
///
/// Grow/shrink weights are dimensionless; retained allocation treats
/// non-finite/negative values as zero.
class Flex {
public:
    template <class Child>
    explicit Flex(Child&& child) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    /// Set dimensionless main-axis growth weight.
    Flex&& grow(float value) && {
        grow_ = value;
        return std::move(*this);
    }

    /// Set dimensionless main-axis shrink weight.
    Flex&& shrink(float value) && {
        shrink_ = value;
        return std::move(*this);
    }

    Spec spec() && {
        const float grow = grow_;
        const float shrink = shrink_;
        return Spec{
            [grow, shrink] { return std::make_unique<FlexComponent>(grow, shrink); },
            std::move(children_)};
    }

private:
    float grow_{};
    float shrink_{};
    std::vector<Spec> children_;
};


/// Detached fixed-size empty layout element.
///
/// Dimensions are logical UI pixels; retained layout normalizes invalid or
/// negative extents to zero.
class Spacer {
public:
    /// Create a vertical spacer with zero width.
    explicit Spacer(float height) : size_{0.0f, height} {}
    /// Create a spacer with explicit logical width/height.
    Spacer(float width, float height) : size_{width, height} {}
    /// Create a spacer from an owned Size value.
    explicit Spacer(Size size) : size_(size) {}

    Spec spec() && {
        const auto size = size_;
        return Spec{[size] { return std::make_unique<SpacerComponent>(size); }, {}};
    }

private:
    Size size_{};
};


/// Detached overlay-stack container; children are owned in paint order and
/// receive the same container bounds.
class Stack {
public:
    template <class... Children>
    explicit Stack(Children&&... children) {
        (children_.push_back(make_spec(std::forward<Children>(children))), ...);
    }

    Spec spec() && {
        return Spec{[] { return std::make_unique<StackComponent>(); }, std::move(children_)};
    }

private:
    std::vector<Spec> children_;
};


/// Detached one-child uniform-padding builder.
///
/// Padding uses logical UI pixels and invalid/negative values normalize to zero.
class Padding {
public:
    template <class Child>
    Padding(float padding, Child&& child) : padding_(padding) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    Spec spec() && {
        const auto padding = padding_;
        return Spec{[padding] { return std::make_unique<PaddingComponent>(padding); },
                    std::move(children_)};
    }

private:
    float padding_{};
    std::vector<Spec> children_;
};


/// Detached horizontal layout builder.
///
/// Defaults: 18 logical-pixel gap, Align::Start, Justify::Start. Fluent setters
/// mutate only detached configuration until spec() && consumes child recipes.
class Row {
public:
    template <class... Children>
    explicit Row(Children&&... children) {
        (children_.push_back(make_spec(std::forward<Children>(children))), ...);
    }

    /// Set logical-pixel inter-child gap; invalid/negative values normalize to zero.
    Row&& gap(float value) && {
        gap_ = value;
        return std::move(*this);
    }

    /// Set cross-axis alignment.
    Row&& align(Align value) && {
        align_ = value;
        return std::move(*this);
    }

    /// Set main-axis justification/distribution.
    Row&& justify(Justify value) && {
        justify_ = value;
        return std::move(*this);
    }

    Spec spec() && {
        const float gap = gap_;
        const auto align = align_;
        const auto justify = justify_;
        return Spec{
            [gap, align, justify] {
                return std::make_unique<RowComponent>(gap, align, justify);
            },
            std::move(children_)};
    }

private:
    float gap_{18.0f};
    Align align_{Align::Start};
    Justify justify_{Justify::Start};
    std::vector<Spec> children_;
};


/// Detached vertical layout builder.
///
/// Defaults: 16 logical-pixel gap, 24 logical-pixel uniform padding,
/// Align::Start and Justify::Start.
class Column {
public:
    template <class... Children>
    explicit Column(Children&&... children) {
        (children_.push_back(make_spec(std::forward<Children>(children))), ...);
    }

    /// Set logical-pixel inter-child gap; invalid/negative values normalize to zero.
    Column&& gap(float value) && {
        gap_ = value;
        return std::move(*this);
    }

    /// Set uniform logical-pixel padding; invalid/negative values normalize to zero.
    Column&& padding(float value) && {
        padding_ = value;
        return std::move(*this);
    }

    /// Set cross-axis alignment.
    Column&& align(Align value) && {
        align_ = value;
        return std::move(*this);
    }

    /// Set main-axis justification/distribution.
    Column&& justify(Justify value) && {
        justify_ = value;
        return std::move(*this);
    }

    Spec spec() && {
        const float gap = gap_;
        const float padding = padding_;
        const auto align = align_;
        const auto justify = justify_;
        return Spec{
            [gap, padding, align, justify] {
                return std::make_unique<ColumnComponent>(gap, padding, align, justify);
            },
            std::move(children_)};
    }

private:
    float gap_{16.0f};
    float padding_{24.0f};
    Align align_{Align::Start};
    Justify justify_{Justify::Start};
    std::vector<Spec> children_;
};


} // namespace ui