#include <nativeui/table_view.hpp>
#include <nativeui/detail/theme_binding.hpp>

#include "detail/table_view_kernel.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace ui::detail {
namespace {
float coordinate(double value) noexcept {
    const double maximum = std::numeric_limits<float>::max();
    return static_cast<float>(std::clamp(value, -maximum, maximum));
}
double bounded_width(const TableColumn &column, double value) noexcept {
    value = std::max(column.minimum_width, value);
    return column.maximum_width ? std::min(value, *column.maximum_width) : value;
}
class TableBodyLayout final : public CollectionLayoutPolicy {
  public:
    [[nodiscard]] CollectionLayoutSnapshot prepare(CollectionHeightSnapshot heights,
                                                   std::size_t count, Rect viewport, Point offset,
                                                   bool) const override {
        if (heights.size() != count)
            throw std::logic_error("TableView height count mismatch");
        CollectionLayoutSnapshot result;
        result.content = {viewport.w, coordinate(heights.total_height())};
        const auto range = heights.range(offset.y, viewport.h, 2);
        result.window.reserve(range.last - range.first);
        for (std::size_t i = range.first; i < range.last; ++i)
            result.window.push_back(i);
        result.geometry = std::make_shared<const CollectionVerticalGeometry>(std::move(heights),
                                                                             viewport, offset);
        return result;
    }
};
enum class HeaderGesture { None, Press, Resize, Reorder, Autofit, Keyboard };
struct HeaderContact {
    std::optional<ColumnId> hovered, active, pressed, insertion;
    std::function<void()> release;
    TableLayout start;
    Point down;
    double start_width{};
    std::uint64_t serial{};
    HeaderGesture gesture{HeaderGesture::None};
    bool mounted{}, focused{};
};
void stop_header(const std::shared_ptr<HeaderContact> &contact) {
    ++contact->serial;
    contact->gesture = HeaderGesture::None;
    contact->pressed.reset();
    contact->insertion.reset();
    auto release = std::exchange(contact->release, {});
    if (release)
        release();
}
void stop_header_noexcept(const std::shared_ptr<HeaderContact> &contact) noexcept {
    try {
        stop_header(contact);
    } catch (...) {
    }
}
class TableSession final : public CollectionColumnsGeometry,
                           public std::enable_shared_from_this<TableSession> {
  public:
    TableSession(TableViewOptions options, ColumnId tree_column)
        : options(std::move(options)), tree_column(std::move(tree_column)) {
        validate_table_columns(this->options.columns);
        if (!std::isfinite(this->options.style.header_height) ||
            this->options.style.header_height <= 0)
            throw std::invalid_argument("table header height must be positive and finite");
        natural_widths.resize(this->options.columns.size());
        sync();
        prepare(320);
        published_geometry = geometry;
    }
    TableViewOptions options;
    ColumnId tree_column;
    TableLayout model;
    std::optional<TableLayout> preview;
    std::optional<SortOrder> current_sort;
    std::shared_ptr<const ResolvedTableGeometry> geometry, published_geometry;
    std::vector<double> natural_widths;
    const Theme *header_theme{};
    std::shared_ptr<HeaderContact> contact{std::make_shared<HeaderContact>()};
    std::optional<Binding<TableLayout>::Subscription> layout_subscription;
    std::optional<Binding<std::optional<SortOrder>>::Subscription> sort_subscription;
    std::function<void()> invalidate_layout, invalidate_paint;
    std::function<bool()> mount_permission;
    std::uint64_t layout_revision{std::numeric_limits<std::uint64_t>::max()},
        sort_revision{std::numeric_limits<std::uint64_t>::max()};
    double viewport_width{320}, offset_x{}, candidate_offset_x{},
        published_viewport_width{320}, published_offset_x{};
    std::uint64_t geometry_generation{};
    bool mounted{}, interactive{true}, read_only{}, dirty{true};

    [[nodiscard]] std::optional<std::size_t> index(const ColumnId &id) const noexcept {
        for (std::size_t i = 0; i < options.columns.size(); ++i)
            if (options.columns[i].id == id)
                return i;
        return {};
    }
    [[nodiscard]] const ResolvedTableColumn *resolved(const ColumnId &id,
                                                     bool published = false) const noexcept {
        const auto &snapshot = published ? published_geometry : geometry;
        if (snapshot)
            for (const auto &column : snapshot->columns)
                if (options.columns[column.declared_index].id == id)
                    return &column;
        return nullptr;
    }
    [[nodiscard]] bool writable_layout() const noexcept {
        return mounted && contact->mounted && interactive && !read_only &&
               (!options.layout || options.layout->valid()) &&
               (!mount_permission || mount_permission());
    }
    [[nodiscard]] bool writable_sort() const noexcept {
        return mounted && contact->mounted && interactive && !read_only &&
               (!options.sort || options.sort->valid()) && (options.sort || options.sort_request) &&
               (!mount_permission || mount_permission());
    }
    [[nodiscard]] std::function<bool()> guard(std::uint64_t serial,
                                            std::function<bool()> input_permission) {
        const std::weak_ptr<TableSession> weak = shared_from_this();
        return [weak, serial, input_permission = std::move(input_permission)] {
            const auto state = weak.lock();
            return state && state->mounted && state->contact->mounted && state->interactive &&
                   !state->read_only && state->contact->serial == serial &&
                   (!state->mount_permission || state->mount_permission()) &&
                   (!input_permission || input_permission());
        };
    }
    void request_layout() {
        dirty = true;
        const auto callback = invalidate_layout;
        if (mounted && callback)
            callback();
    }
    void sync() {
        const auto owner = mounted ? shared_from_this() : std::shared_ptr<TableSession>{};
        bool changed = false;
        if (options.layout && layout_revision != options.layout->revision()) {
            for (unsigned retry = 0; retry < 2; ++retry) {
                const auto before = options.layout->revision();
                auto candidate = options.layout->snapshot();
                if (before != options.layout->revision())
                    continue;
                model = std::move(candidate);
                layout_revision = before;
                preview.reset();
                changed = true;
                if (contact->gesture != HeaderGesture::None)
                    stop_header(contact);
                break;
            }
        }
        if (options.sort && sort_revision != options.sort->revision()) {
            for (unsigned retry = 0; retry < 2; ++retry) {
                const auto before = options.sort->revision();
                auto candidate = options.sort->snapshot();
                if (before != options.sort->revision())
                    continue;
                current_sort = std::move(candidate);
                sort_revision = before;
                changed = true;
                break;
            }
        }
        if (changed || dirty) {
            prepare(published_viewport_width);
            dirty = false;
            if (mounted && changed)
                request_layout();
        }
    }
    void prepare(double width) {
        auto candidate = resolve_table_columns(options.columns, preview ? *preview : model,
                                               std::max(0.0, width));
        const bool changed = !geometry || candidate.columns != geometry->columns;
        auto snapshot = changed
                            ? std::make_shared<const ResolvedTableGeometry>(std::move(candidate))
                            : geometry;
        if (changed)
            ++geometry_generation;
        geometry = std::move(snapshot);
        viewport_width = width;
        candidate_offset_x = std::clamp(offset_x, 0.0, std::max(0.0, geometry->width - width));
    }
    [[nodiscard]] Rect column_bounds(const std::string &id, Rect row) const noexcept override {
        const auto *column = resolved(id);
        if (!column)
            return {};
        return {coordinate(static_cast<double>(row.x) + column->x), row.y,
                coordinate(column->width), row.h};
    }
    [[nodiscard]] Rect published_column_bounds(const std::string &id,
                                               Rect row) const noexcept override {
        const auto *column = resolved(id, true);
        if (!column)
            return {};
        return {coordinate(static_cast<double>(row.x) + column->x), row.y,
                coordinate(column->width), row.h};
    }
    [[nodiscard]] double column_width(const std::string &id) const noexcept override {
        const auto *column = resolved(id);
        return column ? column->width : 0.0;
    }
    void record_natural_width(const std::string &id, double width) noexcept override {
        if (const auto i = index(id); i && std::isfinite(width) && width >= 0)
            natural_widths[*i] = std::max(natural_widths[*i], width);
    }
    [[nodiscard]] std::uint64_t measurement_generation() const noexcept override {
        return geometry_generation;
    }
    [[nodiscard]] std::string_view hierarchy_column() const noexcept override {
        return tree_column;
    }
    [[nodiscard]] double total_width() const noexcept override { return geometry->width; }
    [[nodiscard]] double horizontal_offset() const noexcept override { return candidate_offset_x; }
    [[nodiscard]] double published_horizontal_offset() const noexcept override {
        return published_offset_x;
    }
    void publish_geometry() noexcept {
        // All allocations occurred during the fallible layout preparation.
        published_geometry = geometry;
        published_viewport_width = viewport_width;
        offset_x = candidate_offset_x;
        published_offset_x = candidate_offset_x;
    }
    [[nodiscard]] Align column_alignment(const std::string &id) const noexcept override {
        const auto i = index(id);
        return i ? options.columns[*i].align : Align::Start;
    }
    bool set_horizontal_offset(double value) noexcept override {
        if (!std::isfinite(value))
            return false;
        value = std::clamp(value, 0.0,
                           std::max(0.0, published_geometry->width - published_viewport_width));
        if (value == offset_x)
            return false;
        offset_x = value;
        return true;
    }
    bool publish_layout(TableLayout candidate, const std::function<bool()> &permitted) {
        const auto owner = shared_from_this();
        if (!writable_layout() || !permitted())
            return false;
        const auto before = options.layout ? options.layout->revision() : 0;
        const auto receipt = make_collection_write_receipt();
        request_layout();
        if (!writable_layout() || !permitted())
            return false;
        if (options.layout) {
            auto binding = *options.layout;
            binding.set_if(candidate, [binding, before, permitted, receipt] {
                if (!binding.valid() || binding.revision() != before || !permitted())
                    return false;
                receipt->accepted = true;
                return true;
            });
            if (!receipt->accepted) {
                sync();
                return false;
            }
            if (!binding.valid() || binding.revision() != before + 1 || !permitted())
                return true;
        } else {
            if (candidate == model)
                return false;
            model = candidate;
            receipt->accepted = true;
            prepare(published_viewport_width);
        }
        const auto callback = options.layout_change;
        if (writable_layout() && permitted() &&
            (options.layout ? options.layout->revision() == before + 1 : model == candidate) &&
            callback)
            callback(candidate);
        return receipt->accepted;
    }
    void sort_column(const ColumnId &id, const std::function<bool()> &permitted) {
        const auto owner = shared_from_this();
        const auto i = index(id);
        if (!i || !options.columns[*i].sortable || !writable_sort() || !permitted())
            return;
        SortOrder candidate{id, SortDirection::Ascending};
        if (current_sort && current_sort->column == id &&
            current_sort->direction == SortDirection::Ascending)
            candidate.direction = SortDirection::Descending;
        const auto before = options.sort ? options.sort->revision() : 0;
        const auto invalidate = invalidate_paint;
        if (invalidate)
            invalidate();
        if (!writable_sort() || !permitted())
            return;
        if (options.sort) {
            auto binding = *options.sort;
            const auto receipt = make_collection_write_receipt();
            binding.set_if(candidate, [binding, before, permitted, receipt] {
                if (!binding.valid() || binding.revision() != before || !permitted())
                    return false;
                receipt->accepted = true;
                return true;
            });
            if (!receipt->accepted || !binding.valid() || binding.revision() != before + 1 ||
                !permitted())
                return;
        } else
            current_sort = candidate;
        const auto callback = options.sort_request;
        if (writable_sort() && permitted() &&
            (options.sort ? options.sort->revision() == before + 1 : current_sort == candidate) &&
            callback)
            callback(candidate);
    }
    void autofit(const ColumnId &id, const std::function<bool()> &permitted) {
        const auto owner = shared_from_this();
        const auto i = index(id);
        if (!i || options.columns[*i].fixed || !writable_layout() || !permitted())
            return;
        const auto before = options.layout ? options.layout->revision() : 0;
        const auto provider = options.autofit_width;
        if (!writable_layout() || !permitted() ||
            (options.layout && options.layout->revision() != before))
            return;
        static const Theme fallback = default_theme();
        const auto &theme = header_theme ? *header_theme : fallback;
        TextStyle text;
        text.size = theme.typography.control_size;
        text.family = theme.typography.family;
        text.fallback_families = theme.typography.fallback_families;
        text.weight = theme.typography.control_weight;
        text.slant = theme.typography.slant;
        double width = std::max(
            natural_widths[*i],
            static_cast<double>(TextService::measure(options.columns[*i].title, text).width) +
                16.0);
        if (provider)
            width = provider(id);
        if (!writable_layout() || !permitted() ||
            (options.layout && options.layout->revision() != before))
            return;
        if (!std::isfinite(width) || width < 0)
            throw std::invalid_argument("table autofit provider returned an invalid width");
        auto candidate = model;
        candidate.widths[id] = bounded_width(options.columns[*i], width);
        publish_layout(std::move(candidate), permitted);
    }
};
class TableHeader final : public Component, public ThemeBinding {
  public:
    explicit TableHeader(std::shared_ptr<TableSession> state) : state_(std::move(state)) {}
    void bind_theme(const Theme &theme) noexcept override {
        ThemeBinding::bind_theme(theme);
        state_->header_theme = &theme;
    }
    [[nodiscard]] bool focusable() const noexcept override {
        return !state_->options.columns.empty();
    }
    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }
    [[nodiscard]] bool clips_children() const noexcept override { return true; }
    [[nodiscard]] Size measure(const std::vector<ChildMetrics> &) const override {
        return {coordinate(state_->geometry->width),
                coordinate(state_->options.style.header_height)};
    }
    void mount(MountContext &context) override {
        state_->contact->mounted = true;
        state_->mount_permission = InputMutationAccess::guard(context);
        request_focus_ = context.focus_requester();
    }
    void unmount(LifecycleContext &) override {
        state_->contact->mounted = false;
        stop_header_noexcept(state_->contact);
        request_focus_ = {};
        state_->mount_permission = {};
    }
    void deactivate(LifecycleContext &) override {
        stop_header_noexcept(state_->contact);
        state_->preview.reset();
        state_->contact->hovered.reset();
    }
    void focus_changed(bool focused, FocusContext &context) override {
        const auto state = state_;
        state->contact->focused = focused;
        if (focused && !state->contact->active && !state->published_geometry->columns.empty())
            state->contact->active =
                state->options.columns[state->published_geometry->columns.front().declared_index].id;
        context.invalidate();
    }
    [[nodiscard]] SemanticInfo semantics() const override {
        SemanticInfo info;
        info.role = SemanticRole::Group;
        info.name = "Columns";
        info.focusable = focusable();
        info.focused = state_->contact->focused;
        info.enabled = effective_enabled();
        info.read_only = effective_read_only();
        if (info.enabled)
            info.actions.push_back(SemanticAction::Focus);
        return info;
    }
    EventResult input(const InputEvent &event, InputContext &context) override {
        const auto state = state_;
        const auto contact = state->contact;
        const auto input_permission = InputMutationAccess::guard(context);
        state->interactive = effective_enabled();
        state->read_only = effective_read_only();
        if (event.type == InputType::PointerCancel ||
            (event.type == InputType::KeyDown && event.key == Key::Escape)) {
            const bool armed = contact->gesture != HeaderGesture::None;
            stop_header(contact);
            state->preview.reset();
            state->prepare(state->published_viewport_width);
            state->request_layout();
            return armed ? EventResult::Handled : EventResult::Ignored;
        }
        if (!effective_enabled())
            return EventResult::Ignored;
        if (event.type == InputType::PointerWheel &&
            state->set_horizontal_offset(state->published_offset_x - event.delta.x)) {
            state->request_layout();
            return EventResult::Handled;
        }
        const auto hit = hit_column(event.position, context.bounds());
        if (event.type == InputType::PointerLeave) {
            contact->hovered.reset();
            context.invalidate();
            return EventResult::Ignored;
        }
        if (event.type == InputType::PointerMove) {
            contact->hovered =
                hit ? std::optional<ColumnId>{state->options.columns[*hit].id} : std::nullopt;
            if (contact->pressed && state->writable_layout()) {
                const auto i = state->index(*contact->pressed);
                if (i && !state->options.columns[*i].fixed) {
                    if (contact->gesture == HeaderGesture::Resize) {
                        auto preview = contact->start;
                        preview.widths[*contact->pressed] = bounded_width(
                            state->options.columns[*i],
                            contact->start_width + event.position.x - contact->down.x);
                        state->preview = std::move(preview);
                        state->prepare(state->published_viewport_width);
                        state->request_layout();
                    } else if ((contact->gesture == HeaderGesture::Press ||
                                contact->gesture == HeaderGesture::Reorder) &&
                               std::fabs(event.position.x - contact->down.x) >= 4) {
                        contact->gesture = HeaderGesture::Reorder;
                        contact->insertion =
                            hit ? std::optional<ColumnId>{state->options.columns[*hit].id}
                                : std::nullopt;
                        context.invalidate();
                    }
                }
                return EventResult::Handled;
            }
            context.invalidate();
            return EventResult::Ignored;
        }
        if (event.type == InputType::PointerDown) {
            if (!hit || state->read_only)
                return EventResult::Ignored;
            const auto i = hit;
            const auto &hit_id = state->options.columns[*hit].id;
            const auto *resolved = state->resolved(hit_id, true);
            if (!resolved)
                return EventResult::Ignored;
            const double width = resolved->width;
            const double edge =
                context.bounds().x + resolved->x + resolved->width - state->published_offset_x;
            const auto start = state->model;
            auto release = context.pointer_releaser();
            const auto focus = request_focus_;
            stop_header(contact);
            contact->pressed = hit_id;
            contact->active = hit_id;
            contact->start = start;
            contact->down = event.position;
            contact->start_width = width;
            contact->release = std::move(release);
            const bool resize =
                !state->options.columns[*i].fixed && std::fabs(event.position.x - edge) <= 6;
            contact->gesture =
                resize ? (event.clicks > 1 ? HeaderGesture::Autofit : HeaderGesture::Resize)
                       : HeaderGesture::Press;
            const auto serial = contact->serial;
            if (focus)
                focus();
            if (!state->mounted || !contact->mounted || contact->serial != serial ||
                !state->guard(serial, input_permission)()) {
                stop_header(contact);
                return EventResult::Handled;
            }
            context.capture_pointer();
            context.invalidate();
            return EventResult::Handled;
        }
        if (event.type == InputType::PointerUp) {
            auto pressed = std::move(contact->pressed);
            const auto gesture = contact->gesture;
            auto insertion = std::move(contact->insertion);
            auto preview = std::move(state->preview);
            auto start = std::move(contact->start);
            const bool same = pressed && hit && *pressed == state->options.columns[*hit].id;
            stop_header(contact);
            state->preview.reset();
            auto candidate = preview ? std::move(*preview) : std::move(start);
            const auto permitted = state->guard(contact->serial, input_permission);
            if (!pressed || !permitted())
                return pressed ? EventResult::Handled : EventResult::Ignored;
            if (gesture == HeaderGesture::Resize)
                state->publish_layout(std::move(candidate), permitted);
            else if (gesture == HeaderGesture::Autofit)
                state->autofit(*pressed, permitted);
            else if (gesture == HeaderGesture::Reorder) {
                std::vector<ColumnId> order;
                order.reserve(state->published_geometry->columns.size());
                for (const auto &column : state->published_geometry->columns) {
                    const auto &id = state->options.columns[column.declared_index].id;
                    if (id != *pressed)
                        order.push_back(id);
                }
                auto position =
                    insertion ? std::find(order.begin(), order.end(), *insertion) : order.end();
                order.insert(position, *pressed);
                candidate.order = std::move(order);
                state->publish_layout(std::move(candidate), permitted);
            } else if (gesture == HeaderGesture::Press && same)
                state->sort_column(*pressed, permitted);
            state->prepare(state->published_viewport_width);
            return EventResult::Handled;
        }
        if (event.type == InputType::KeyUp && event.key == Key::Space &&
            contact->gesture == HeaderGesture::Keyboard) {
            auto id = std::move(contact->pressed);
            stop_header(contact);
            if (id)
                state->sort_column(*id, state->guard(contact->serial, input_permission));
            return EventResult::Handled;
        }
        if (event.type != InputType::KeyDown || !contact->active)
            return EventResult::Ignored;
        if (event.key == Key::Enter) {
            stop_header(contact);
            const auto id = *contact->active;
            state->sort_column(id, state->guard(contact->serial, input_permission));
            return EventResult::Handled;
        }
        if (event.key == Key::Space) {
            if (contact->gesture != HeaderGesture::Keyboard) {
                stop_header(contact);
                contact->gesture = HeaderGesture::Keyboard;
                contact->pressed = contact->active;
            }
            return EventResult::Handled;
        }
        if (event.key == Key::Left || event.key == Key::Right || event.key == Key::Home ||
            event.key == Key::End) {
            const auto &columns = state->published_geometry->columns;
            for (std::size_t i = 0; i < columns.size(); ++i)
                if (state->options.columns[columns[i].declared_index].id == *contact->active) {
                    const auto target = event.key == Key::Home  ? 0
                                        : event.key == Key::End ? columns.size() - 1
                                        : event.key == Key::Left
                                            ? i - std::min<std::size_t>(i, 1)
                                            : std::min(i + 1, columns.size() - 1);
                    contact->active = state->options.columns[columns[target].declared_index].id;
                    break;
                }
            context.invalidate();
            return EventResult::Handled;
        }
        return EventResult::Ignored;
    }
    void paint(PaintContext &context) const override {
        const auto state = state_;
        const auto &theme = current_theme();
        const auto &style = state->options.style;
        const auto clip = context.painter().scoped_clip(context.bounds());
        context.painter().fill_rounded_rect(
            context.bounds(), 0, style.header_fill.value_or(theme.palette.control_background));
        for (const auto &resolved : state->published_geometry->columns) {
            const auto &column = state->options.columns[resolved.declared_index];
            const Rect rect{coordinate(context.bounds().x + resolved.x - state->published_offset_x),
                            context.bounds().y, coordinate(resolved.width), context.bounds().h};
            context.painter().text({coordinate(rect.x + 4), coordinate(rect.y + rect.h * 0.7)},
                                   column.title, theme.typography.control_size,
                                   style.header_text.value_or(theme.palette.text));
            context.painter().fill_rounded_rect(
                {coordinate(rect.x + rect.w - 1), rect.y, 1, rect.h}, 0,
                style.separator.value_or(theme.palette.border));
            if (state->current_sort && state->current_sort->column == column.id)
                context.painter().text(
                    {coordinate(rect.x + rect.w - 14), coordinate(rect.y + rect.h * 0.7)},
                    state->current_sort->direction == SortDirection::Ascending ? "^" : "v", 11,
                    theme.palette.accent);
            if (state->contact->focused && state->contact->active == column.id)
                context.painter().stroke_rounded_rect(rect, 0, 1, theme.palette.focus);
        }
        if (state->contact->pressed && state->contact->gesture == HeaderGesture::Resize)
            if (const auto *column = state->resolved(*state->contact->pressed, true))
                context.painter().fill_rounded_rect(
                    {coordinate(context.bounds().x + column->x + column->width -
                                state->published_offset_x - 1),
                     context.bounds().y, 2, context.bounds().h},
                    0, style.guide.value_or(theme.palette.accent));
        if (state->contact->insertion)
            if (const auto *column = state->resolved(*state->contact->insertion, true))
                context.painter().fill_rounded_rect(
                    {coordinate(context.bounds().x + column->x - state->published_offset_x - 1),
                     context.bounds().y, 2, context.bounds().h},
                    0, style.guide.value_or(theme.palette.accent));
    }

  private:
    [[nodiscard]] std::optional<std::size_t> hit_column(Point point, Rect bounds) const noexcept {
        if (!bounds.contains(point))
            return {};
        const double x = point.x - bounds.x + state_->published_offset_x;
        for (const auto &column : state_->published_geometry->columns)
            if (x >= column.x && x <= column.x + column.width)
                return column.declared_index;
        return {};
    }
    void effective_availability_changed(const ComponentAvailability &,
                                        const ComponentAvailability &current) noexcept override {
        state_->interactive = current.interactive();
        state_->read_only = current.read_only;
        if (!current.interactive() || current.read_only) {
            stop_header_noexcept(state_->contact);
            state_->preview.reset();
        }
    }
    std::shared_ptr<TableSession> state_;
    std::function<void()> request_focus_;
};
class TableComponent final : public Component {
  public:
    TableComponent(std::shared_ptr<TableSession> state, CollectionSourceFactory source,
                   CollectionHierarchyOptions hierarchy,
                   std::shared_ptr<CollectionLayoutPolicy> layout)
        : state_(std::move(state)), source_(std::move(source)), hierarchy_(hierarchy),
          layout_(std::move(layout)) {}
    [[nodiscard]] bool uses_retained_checkpoint() const noexcept override { return true; }
    [[nodiscard]] std::vector<Spec> children() {
        const auto state = state_;
        std::vector<std::string> ids;
        ids.reserve(state->options.columns.size());
        for (const auto &column : state->options.columns)
            ids.push_back(column.id);
        auto options = state->options.rows;
        options.empty_content = ids.empty();
        return {
            Spec{[state] { return std::make_unique<TableHeader>(state); }, {}},
            make_collection_view(source_, std::move(options), hierarchy_, layout_, std::move(ids))};
    }
    [[nodiscard]] Size measure(const std::vector<ChildMetrics> &children) const override {
        const auto body = children.size() > 1 ? children[1].preferred : Size{};
        return {std::max(coordinate(state_->geometry->width), body.w),
                coordinate(state_->options.style.header_height + body.h)};
    }
    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics> &) const override { return {}; }
    [[nodiscard]] Constraints child_constraints(const Constraints &constraints, std::size_t index,
                                                std::size_t) const override {
        if (constraints.bounded_width())
            state_->prepare(constraints.max.w);
        return index == 0
                   ? Constraints{{0, 0},
                                 {constraints.max.w,
                                  coordinate(state_->options.style.header_height)}}
                   : Constraints{
                         {0, 0},
                         {constraints.max.w,
                          std::isfinite(constraints.max.h)
                              ? coordinate(std::max(0.0, constraints.max.h -
                                                             state_->options.style.header_height))
                              : kUnboundedExtent}};
    }
    void layout_children(Rect bounds, const std::vector<ChildMetrics> &,
                         std::vector<ChildPlacement> &placements) const override {
        state_->prepare(bounds.w);
        const float header = std::min(bounds.h, coordinate(state_->options.style.header_height));
        if (!placements.empty())
            placements[0].bounds = {bounds.x, bounds.y, bounds.w, header};
        if (placements.size() > 1)
            placements[1].bounds = {bounds.x, coordinate(bounds.y + header), bounds.w,
                                    std::max(0.0f, bounds.h - header)};
    }
    void mount(MountContext &context) override {
        const auto state = state_;
        state->mounted = true;
        state->invalidate_layout = context.layout_invalidator();
        state->invalidate_paint = context.invalidator();
        const std::weak_ptr<TableSession> weak = state;
        if (state->options.layout) {
            auto binding = *state->options.layout;
            state->layout_subscription = binding.observe([weak](const auto &) {
                if (const auto current = weak.lock())
                    current->request_layout();
            });
        }
        if (state->options.sort) {
            auto binding = *state->options.sort;
            state->sort_subscription = binding.observe([weak](const auto &) {
                if (const auto current = weak.lock())
                    current->request_layout();
            });
        }
        state->sync();
    }
    void unmount(LifecycleContext &) override {
        const auto state = state_;
        state->mounted = false;
        stop_header_noexcept(state->contact);
        state->layout_subscription.reset();
        state->sort_subscription.reset();
        state->invalidate_layout = {};
        state->invalidate_paint = {};
    }
    [[nodiscard]] SemanticInfo semantics() const override {
        SemanticInfo info;
        info.role = SemanticRole::Group;
        info.enabled = effective_enabled();
        info.read_only = effective_read_only();
        return info;
    }
    void paint(PaintContext &) const override {}

  private:
    void layout_committed(Rect, Rect) noexcept override { state_->publish_geometry(); }
    void retained_checkpoint() override { state_->sync(); }
    void bind_descendant_context(Component &component) const override {
        if (auto *participant = dynamic_cast<CollectionColumnsParticipant *>(&component))
            participant->bind_collection_columns(state_);
    }
    std::shared_ptr<TableSession> state_;
    CollectionSourceFactory source_;
    CollectionHierarchyOptions hierarchy_;
    std::shared_ptr<CollectionLayoutPolicy> layout_;
};
} // namespace
void validate_table_columns(const std::vector<TableColumn> &columns) {
    std::unordered_set<std::string> ids;
    for (const auto &column : columns) {
        if (column.id.empty() || !ids.insert(column.id).second)
            throw std::invalid_argument("table column IDs must be nonempty and unique");
        if (!std::isfinite(column.width) || column.width < 0 ||
            !std::isfinite(column.minimum_width) || column.minimum_width <= 0 ||
            (column.maximum_width && (!std::isfinite(*column.maximum_width) ||
                                      *column.maximum_width < column.minimum_width)))
            throw std::invalid_argument("table column widths must be finite and consistent");
    }
}
ResolvedTableGeometry resolve_table_columns(const std::vector<TableColumn> &columns,
                                            const TableLayout &layout, double available) {
    std::unordered_map<std::string, std::size_t> indices;
    for (std::size_t i = 0; i < columns.size(); ++i)
        indices.emplace(columns[i].id, i);
    std::vector<std::size_t> moving;
    std::vector<bool> seen(columns.size(), false);
    for (const auto &id : layout.order)
        if (const auto found = indices.find(id);
            found != indices.end() && !columns[found->second].fixed && !seen[found->second]) {
            moving.push_back(found->second);
            seen[found->second] = true;
        }
    for (std::size_t i = 0; i < columns.size(); ++i)
        if (!columns[i].fixed && !seen[i])
            moving.push_back(i);
    std::vector<std::size_t> order;
    order.reserve(columns.size());
    std::size_t next{};
    for (std::size_t i = 0; i < columns.size(); ++i)
        order.push_back(columns[i].fixed ? i : moving[next++]);
    std::vector<double> widths(columns.size());
    double committed{};
    std::size_t automatic{};
    for (const auto index : order) {
        const auto &column = columns[index];
        double width = column.width;
        if (!column.fixed)
            if (const auto external = layout.widths.find(column.id);
                external != layout.widths.end() && std::isfinite(external->second) &&
                external->second > 0)
                width = external->second;
        if (width == 0) {
            widths[index] = column.minimum_width;
            committed += column.minimum_width;
            ++automatic;
        } else {
            widths[index] = bounded_width(column, width);
            committed += widths[index];
        }
    }
    double remainder = std::max(0.0, available - committed);
    // Bounded water filling leaves max-constrained tracks at their maximum.
    for (std::size_t pass = 0; pass < columns.size() && automatic && remainder > 0; ++pass) {
        const double share = remainder / static_cast<double>(automatic);
        double distributed{};
        std::size_t recipients{};
        for (const auto index : order) {
            const auto &column = columns[index];
            const auto override = layout.widths.find(column.id);
            const bool external = !column.fixed && override != layout.widths.end() &&
                                  std::isfinite(override->second) && override->second > 0;
            if (column.width != 0 || external ||
                (column.maximum_width && widths[index] >= *column.maximum_width))
                continue;
            const double gain = column.maximum_width
                                    ? std::min(share, *column.maximum_width - widths[index])
                                    : share;
            widths[index] += gain;
            distributed += gain;
            if (!column.maximum_width || widths[index] < *column.maximum_width)
                ++recipients;
        }
        if (!(distributed > 0))
            break;
        remainder = std::max(0.0, remainder - distributed);
        automatic = recipients;
    }
    ResolvedTableGeometry result;
    result.columns.reserve(columns.size());
    for (const auto index : order) {
        result.columns.push_back({index, result.width, widths[index]});
        result.width += widths[index];
    }
    if (!std::isfinite(result.width))
        throw std::overflow_error("table width overflow");
    return result;
}
Spec make_retained_table(CollectionSourceFactory source, TableViewOptions options,
                         CollectionHierarchyOptions hierarchy, ColumnId tree_column,
                         std::shared_ptr<CollectionLayoutPolicy> body_layout) {
    validate_table_columns(options.columns);
    Spec result;
    result.factory = [source = std::move(source), options = std::move(options), hierarchy,
                      tree_column = std::move(tree_column), body_layout = std::move(body_layout)] {
        auto state = std::make_shared<TableSession>(options, tree_column);
        return std::make_unique<TableComponent>(std::move(state), source, hierarchy, body_layout);
    };
    result.children_factory = [](Component &component) {
        return static_cast<TableComponent &>(component).children();
    };
    return result;
}
Spec make_table_view(CollectionSourceFactory source, TableViewOptions options) {
    return make_retained_table(std::move(source), std::move(options),
                               CollectionHierarchyOptions{false, true, false, 0, 0}, {},
                               std::make_shared<TableBodyLayout>());
}
} // namespace ui::detail
