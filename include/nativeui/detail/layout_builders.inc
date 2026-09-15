namespace detail {

// Scroll's public builder borrows ScrollState, while the retained tree may outlive
// that owner. Keep one weak-control subscription alive as a lifetime token and
// never enter the legacy ScrollComponent once the state has been invalidated.
// This closes both the spec-before-realization and post-realization teardown paths
// without making the retained tree own the application ScrollState.
class RetainedScrollComponent final : public Component {
public:
    RetainedScrollComponent(
        ScrollAxis axis,
        std::shared_ptr<ScrollState::Subscription> lifetime,
        std::unique_ptr<ScrollComponent> delegate)
        : axis_(axis),
          lifetime_(std::move(lifetime)),
          delegate_(std::move(delegate)) {}

    [[nodiscard]] bool clips_children() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        if (alive()) return delegate_->measure(children);
        return children.empty() ? Size{} : children.front().preferred;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        if (alive()) return delegate_->minimum_size(children);
        if (children.empty()) return {};
        auto minimum = children.front().minimum;
        if (axis_ == ScrollAxis::Horizontal || axis_ == ScrollAxis::Both) minimum.w = 0.0f;
        if (axis_ == ScrollAxis::Vertical || axis_ == ScrollAxis::Both) minimum.h = 0.0f;
        return minimum;
    }

    [[nodiscard]] Constraints child_constraints(
        const Constraints& constraints, std::size_t index, std::size_t count) const override {
        if (alive()) return delegate_->child_constraints(constraints, index, count);
        Size maximum = constraints.max;
        if (axis_ == ScrollAxis::Horizontal || axis_ == ScrollAxis::Both) {
            maximum.w = kUnboundedExtent;
        }
        if (axis_ == ScrollAxis::Vertical || axis_ == ScrollAxis::Both) {
            maximum.h = kUnboundedExtent;
        }
        return Constraints::loose(maximum);
    }

    void mount(MountContext& context) override {
        if (alive()) delegate_->mount(context);
    }

    void unmount(LifecycleContext& context) override {
        // ScrollComponent::unmount only resets its weak subscription and remains
        // safe after ScrollState invalidation.
        if (delegate_) delegate_->unmount(context);
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>& children,
        std::vector<ChildPlacement>& placements) const override {
        if (alive()) {
            delegate_->layout_children(bounds, children, placements);
            return;
        }
        if (placements.empty() || children.empty()) return;

        Size content = children.front().preferred;
        if (axis_ == ScrollAxis::Vertical) content.w = std::max(content.w, bounds.w);
        if (axis_ == ScrollAxis::Horizontal) content.h = std::max(content.h, bounds.h);
        placements.front().bounds = Rect{bounds.x, bounds.y, content.w, content.h};
    }

    void paint(PaintContext& context) const override {
        if (alive()) delegate_->paint(context);
    }

private:
    [[nodiscard]] bool alive() const noexcept {
        return delegate_ && lifetime_ && lifetime_->active();
    }

    ScrollAxis axis_{ScrollAxis::Vertical};
    std::shared_ptr<ScrollState::Subscription> lifetime_;
    std::unique_ptr<ScrollComponent> delegate_;
};

} // namespace detail

class Scroll {
public:
    template <class Child>
    Scroll(ScrollState& state, Child&& child) : state_(&state) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    Spec spec() && {
        auto* state = state_;
        const auto axis = state_->axis();
        auto lifetime = std::make_shared<ScrollState::Subscription>(
            state_->observe([](Point) {}));
        return Spec{
            [state, axis, lifetime] {
                std::unique_ptr<ScrollComponent> delegate;
                if (lifetime && lifetime->active()) {
                    delegate = std::make_unique<ScrollComponent>(*state);
                }
                return std::make_unique<detail::RetainedScrollComponent>(
                    axis, lifetime, std::move(delegate));
            },
            std::move(children_)};
    }

private:
    ScrollState* state_{};
    std::vector<Spec> children_;
};

class Clip {
public:
    template <class Child>
    explicit Clip(Child&& child) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    Spec spec() && {
        return Spec{[] { return std::make_unique<ClipComponent>(); }, std::move(children_)};
    }

private:
    std::vector<Spec> children_;
};


class Grid {
public:
    template <class... Children>
    Grid(GridTracks tracks, Children&&... children) : tracks_(std::move(tracks)) {
        (children_.push_back(make_spec(std::forward<Children>(children))), ...);
    }

    Grid&& gap(float value) && {
        column_gap_ = value;
        row_gap_ = value;
        return std::move(*this);
    }

    Grid&& column_gap(float value) && {
        column_gap_ = value;
        return std::move(*this);
    }

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


class Flex {
public:
    template <class Child>
    explicit Flex(Child&& child) {
        children_.push_back(make_spec(std::forward<Child>(child)));
    }

    Flex&& grow(float value) && {
        grow_ = value;
        return std::move(*this);
    }

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


class Spacer {
public:
    explicit Spacer(float height) : size_{0.0f, height} {}
    Spacer(float width, float height) : size_{width, height} {}
    explicit Spacer(Size size) : size_(size) {}

    Spec spec() && {
        const auto size = size_;
        return Spec{[size] { return std::make_unique<SpacerComponent>(size); }, {}};
    }

private:
    Size size_{};
};


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


class Row {
public:
    template <class... Children>
    explicit Row(Children&&... children) {
        (children_.push_back(make_spec(std::forward<Children>(children))), ...);
    }

    Row&& gap(float value) && {
        gap_ = value;
        return std::move(*this);
    }

    Row&& align(Align value) && {
        align_ = value;
        return std::move(*this);
    }

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


class Column {
public:
    template <class... Children>
    explicit Column(Children&&... children) {
        (children_.push_back(make_spec(std::forward<Children>(children))), ...);
    }

    Column&& gap(float value) && {
        gap_ = value;
        return std::move(*this);
    }

    Column&& padding(float value) && {
        padding_ = value;
        return std::move(*this);
    }

    Column&& align(Align value) && {
        align_ = value;
        return std::move(*this);
    }

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