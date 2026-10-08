#include <nativeui/detail/collection_view_kernel.hpp>
#include <nativeui/detail/dynamic_source.hpp>
#include <nativeui/detail/theme_binding.hpp>

#include "detail/collection_tree_graph.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <stdexcept>
#include <unordered_map>

namespace ui::detail {
namespace {
float coordinate(double value) noexcept {
    const double limit = std::numeric_limits<float>::max();
    return static_cast<float>(std::clamp(value, -limit, limit));
}
float extent(double value) noexcept { return coordinate(std::max(0.0, value)); }
double nonnegative(float value) noexcept {
    return std::isfinite(value) ? std::max(0.0, static_cast<double>(value)) : 0.0;
}
std::string row_key(CollectionToken token, std::uint64_t payload) {
    return "collection:" + std::to_string(token) + ":" + std::to_string(payload);
}
struct CollectionDataset {
    std::shared_ptr<const CollectionInput> input;
    CollectionSourceVersion version;
    std::shared_ptr<const CollectionTokenSnapshot> tokens;
    std::shared_ptr<const CollectionTreeGraph> graph;
    std::vector<CollectionVisibleRow> visible;
    std::vector<CollectionToken> order, expanded;
    std::vector<CollectionSearchItem> search;
    std::vector<bool> eligible;
    std::unordered_map<CollectionToken, std::size_t> visible_index;
    std::unordered_map<CollectionToken, std::uint64_t> payloads;
    VirtualSemanticChildren::MetadataSnapshot metadata;
};
struct CollectionContact {
    std::optional<CollectionToken> hovered, pressed, active, insertion;
    std::function<void()> release;
    Point down{};
    std::uint64_t serial{};
    bool disclosure{}, dragging{}, focused{}, mounted{};
};
void stop_contact(const std::shared_ptr<CollectionContact> &contact) {
    ++contact->serial;
    contact->pressed.reset();
    contact->disclosure = false;
    contact->dragging = false;
    contact->insertion.reset();
    auto release = std::exchange(contact->release, {});
    if (release)
        release();
}
void stop_contact_noexcept(const std::shared_ptr<CollectionContact> &contact) noexcept {
    try {
        stop_contact(contact);
    } catch (...) {
    }
}
class CollectionRuntime;
class CollectionRow final : public Component {
  public:
    CollectionRow(std::shared_ptr<CollectionRuntime> runtime, CollectionToken token)
        : runtime_(std::move(runtime)), token_(token) {}
    [[nodiscard]] ComponentAvailability local_availability() const noexcept override;
    [[nodiscard]] Size measure(const std::vector<ChildMetrics> &children) const override;
    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics> &children) const override;
    void layout_children(Rect bounds, const std::vector<ChildMetrics> &children,
                         std::vector<ChildPlacement> &placements) const override;
    [[nodiscard]] Constraints child_constraints(const Constraints &, std::size_t,
                                                std::size_t) const override;
    [[nodiscard]] bool clips_children() const noexcept override { return true; }
    [[nodiscard]] SemanticInfo semantics() const override;
    void paint(PaintContext &) const override {}

  private:
    void bind_descendant_context(Component &component) const override;
    std::shared_ptr<CollectionRuntime> runtime_;
    CollectionToken token_{};
};
class CollectionRuntime final : public CollectionViewSession,
                                public std::enable_shared_from_this<CollectionRuntime> {
  public:
    CollectionRuntime(std::shared_ptr<CollectionSource> source, CollectionViewOptions options,
                      CollectionHierarchyOptions hierarchy,
                      std::shared_ptr<CollectionLayoutPolicy> layout,
                      std::vector<std::string> columns)
        : source(std::move(source)), options(std::move(options)), hierarchy(hierarchy),
          layout(std::move(layout)), columns(std::move(columns)) {
        if (!this->source || !this->layout)
            throw std::invalid_argument("collection source/layout missing");
        if (!std::isfinite(this->options.row_heights.estimate) ||
            this->options.row_heights.estimate <= 0.0 || !std::isfinite(hierarchy.indentation) ||
            hierarchy.indentation < 0.0 || !std::isfinite(hierarchy.chevron_width) ||
            hierarchy.chevron_width < 0.0)
            throw std::invalid_argument("collection geometry must be finite and nonnegative");
        sync(true);
    }
    std::shared_ptr<CollectionSource> source;
    CollectionViewOptions options;
    CollectionHierarchyOptions hierarchy;
    std::shared_ptr<CollectionLayoutPolicy> layout;
    std::vector<std::string> columns;
    std::shared_ptr<CollectionColumnsGeometry> columns_geometry;
    CollectionTokens token_registry;
    std::shared_ptr<const CollectionDataset> dataset;
    CollectionHeightIndex heights;
    CollectionSelection selection;
    std::shared_ptr<const std::vector<VirtualSemanticItemToken>> selected_tokens{
        std::make_shared<const std::vector<VirtualSemanticItemToken>>()};
    std::uint64_t selection_revision{}, epoch{}, published_epoch{}, candidate_epoch{},
        next_payload{1};
    std::optional<CollectionSourceVersion> rejected;
    std::shared_ptr<CollectionContact> contact{std::make_shared<CollectionContact>()};
    std::unique_ptr<CollectionSubscription> subscription;
    CollectionTypeahead typeahead;
    Dispatcher dispatcher;
    TimerHandle typeahead_timer;
    std::uint64_t search_serial{};
    Rect viewport{0, 0, 320, 240};
    Point offset{}, published_offset{}, candidate_offset{};
    std::optional<float> measured_width;
    std::uint64_t measured_columns{};
    CollectionLayoutSnapshot published, candidate;
    std::shared_ptr<const CollectionDataset> published_dataset, candidate_dataset;
    VirtualSemanticChildren::MetadataSnapshot semantic_metadata, published_metadata,
        candidate_metadata;
    const CollectionDataset *semantic_dataset{};
    ComponentAvailability semantic_availability;
    bool semantic_rows_valid{}, semantic_selection_valid{}, semantic_expansion_valid{};
    std::shared_ptr<const std::vector<VirtualSemanticItemToken>> published_selected,
        candidate_selected;
    std::vector<std::size_t> retained_window;
    std::map<std::string, Spec> built;
    std::function<void()> invalidate_layout, invalidate_paint, invalidate_availability,
        invalidate_structure, request_focus;
    std::function<bool()> mount_permission;
    const Theme *theme{};
    ComponentAvailability availability;
    unsigned pending{};
    bool syncing{}, preparing_models{}, selection_dirty{true};

    [[nodiscard]] bool eligible(CollectionToken token) const noexcept {
        if (!dataset)
            return false;
        const auto found = dataset->visible_index.find(token);
        return found != dataset->visible_index.end() && dataset->eligible[found->second];
    }
    [[nodiscard]] std::optional<std::size_t> visible_index(CollectionToken token) const noexcept {
        if (!dataset)
            return {};
        const auto found = dataset->visible_index.find(token);
        return found == dataset->visible_index.end() ? std::nullopt
                                                     : std::optional<std::size_t>{found->second};
    }
    [[nodiscard]] std::optional<std::size_t> dataset_index(CollectionToken token) const noexcept {
        return dataset ? CollectionTokens::index_for_token(*dataset->tokens, token) : std::nullopt;
    }
    [[nodiscard]] bool mutable_available() const noexcept {
        return contact->mounted && availability.interactive() && !availability.read_only &&
               source->valid() && (!mount_permission || mount_permission());
    }
    [[nodiscard]] bool writable() const noexcept {
        return mutable_available() && source->selection_valid();
    }
    [[nodiscard]] ResolvedListViewStyle row_style(CollectionToken token) const noexcept {
        static const Theme fallback = default_theme();
        VisualState state;
        state.enabled = availability.enabled && eligible(token);
        state.read_only = availability.read_only || !source->valid() || !source->selection_valid();
        state.focused = contact->focused && contact->active == token;
        state.selected =
            std::binary_search(selected_tokens->begin(), selected_tokens->end(), token);
        state.hovered = contact->hovered == token;
        state.pressed = contact->pressed == token;
        return resolve_list_view_style(default_list_view_style(theme ? *theme : fallback),
                                       options.style, state);
    }
    void changed_presentation(CollectionToken token, const ResolvedListViewStyle &before) {
        const auto after = row_style(token);
        if (before == after)
            return;
        if (before.row_horizontal_inset != after.row_horizontal_inset ||
            before.row_vertical_inset != after.row_vertical_inset)
            mark(2);
        else
            mark(8);
    }
    void stop_contact_presented() {
        const auto token = contact->pressed;
        const auto before =
            token ? std::optional<ResolvedListViewStyle>{row_style(*token)} : std::nullopt;
        stop_contact(contact);
        if (token && before && contact->mounted)
            changed_presentation(*token, *before);
    }
    void update_hover(std::optional<CollectionToken> next) {
        if (contact->hovered == next)
            return;
        const auto old = contact->hovered;
        const auto old_before =
            old ? std::optional<ResolvedListViewStyle>{row_style(*old)} : std::nullopt;
        const auto next_before =
            next ? std::optional<ResolvedListViewStyle>{row_style(*next)} : std::nullopt;
        contact->hovered = next;
        bool paint = false, layout_dirty = false;
        const auto compare = [&](const auto &before, const auto &token) {
            if (!before || !token)
                return;
            const auto after = row_style(*token);
            paint = paint || after != *before;
            layout_dirty = layout_dirty ||
                           after.row_horizontal_inset != before->row_horizontal_inset ||
                           after.row_vertical_inset != before->row_vertical_inset;
        };
        compare(old_before, old);
        compare(next_before, next);
        if (layout_dirty)
            mark(2);
        else if (paint)
            mark(8);
    }
    void prepare_semantics() {
        if (!dataset)
            return;
        const bool rows_valid = source->valid(), selection_valid = source->selection_valid(),
                   expansion_valid = source->expansion_valid();
        if (semantic_dataset == dataset.get() && semantic_availability == availability &&
            semantic_rows_valid == rows_valid && semantic_selection_valid == selection_valid &&
            semantic_expansion_valid == expansion_valid)
            return;
        auto metadata = std::make_shared<VirtualSemanticChildren::Metadata>(*dataset->metadata);
        for (auto &item : *metadata) {
            item.enabled = item.enabled && availability.interactive();
            item.read_only = availability.read_only || !rows_valid;
            item.actions.clear();
            if (item.enabled)
                item.actions.push_back(SemanticAction::Focus);
            if (item.enabled && !item.read_only) {
                if (selection_valid)
                    item.actions.push_back(SemanticAction::Select);
                item.actions.push_back(SemanticAction::Activate);
                if (expansion_valid && item.expanded != SemanticExpandedState::NotApplicable)
                    item.actions.push_back(item.expanded == SemanticExpandedState::Expanded
                                               ? SemanticAction::Collapse
                                               : SemanticAction::Expand);
            }
        }
        semantic_metadata = std::move(metadata);
        semantic_dataset = dataset.get();
        semantic_availability = availability;
        semantic_rows_valid = rows_valid;
        semantic_selection_valid = selection_valid;
        semantic_expansion_valid = expansion_valid;
        if (published_dataset == dataset) {
            published_metadata = semantic_metadata;
            published_selected = selected_tokens;
        }
        pending |= 8;
    }
    [[nodiscard]] std::function<bool()> guard(std::uint64_t serial, std::uint64_t generation,
                                            std::function<bool()> input_permission) {
        const std::weak_ptr<CollectionRuntime> weak = shared_from_this();
        return [weak, serial, generation, input_permission = std::move(input_permission)] {
            const auto current = weak.lock();
            return current && current->mutable_available() && current->contact->serial == serial &&
                   current->epoch == generation && (!input_permission || input_permission());
        };
    }
    void clear_typeahead() noexcept {
        ++search_serial;
        typeahead.clear();
        const auto timer = std::exchange(typeahead_timer, {});
        if (timer)
            try {
                (void)dispatcher.cancel(timer);
            } catch (...) {
            }
    }
    void expire_typeahead() {
        const auto owner = shared_from_this();
        const auto serial = ++search_serial;
        const auto old = std::exchange(typeahead_timer, {});
        if (old)
            (void)dispatcher.cancel(old);
        if (!dispatcher.valid())
            return;
        const std::weak_ptr<CollectionRuntime> weak = owner;
        try {
            auto timer =
                dispatcher.schedule_after(std::chrono::duration<double>{0.7}, [weak, serial] {
                    const auto current = weak.lock();
                    if (!current || !current->contact->mounted || current->search_serial != serial)
                        return;
                    current->typeahead_timer = {};
                    current->typeahead.clear();
                });
            if (contact->mounted && search_serial == serial)
                typeahead_timer = timer;
            else if (timer)
                (void)dispatcher.cancel(timer);
        } catch (...) {
            typeahead.clear();
            throw;
        }
    }
    void mark(unsigned bits) {
        pending |= bits;
        flush_effects();
    }
    void flush_effects() {
        if (!contact->mounted || syncing || preparing_models)
            return;
        syncing = true;
        const auto current_epoch = epoch;
        const auto run = [&](unsigned bit, const std::function<void()> &handle) {
            if (!contact->mounted || epoch != current_epoch || !(pending & bit))
                return;
            const auto callback = handle;
            pending &= ~bit;
            if (callback)
                callback();
        };
        try {
            run(1, invalidate_structure);
            run(2, invalidate_layout);
            run(4, invalidate_availability);
            run(8, invalidate_paint);
        } catch (...) {
            syncing = false;
            throw;
        }
        syncing = false;
    }
    void sync(bool initial = false) {
        const auto owner = initial ? std::shared_ptr<CollectionRuntime>{} : shared_from_this();
        if (syncing || preparing_models)
            return;
        preparing_models = true;
        struct PreparationGuard {
            bool &flag;
            ~PreparationGuard() { flag = false; }
        } preparation{preparing_models};
        const auto wanted = source->version();
        if ((!dataset || dataset->version != wanted) && (!rejected || *rejected != wanted)) {
            for (unsigned attempt = 0; attempt < 2; ++attempt) {
                const auto attempted_version = source->version();
                const bool rows_changed =
                    !dataset || dataset->version.rows != source->version().rows;
                try {
                    auto prepared = std::make_shared<CollectionDataset>();
                    std::optional<PreparedCollectionTokens> tokens;
                    if (rows_changed) {
                        auto input = source->read();
                        if (!input)
                            continue;
                        tokens = token_registry.prepare(input->keys);
                        prepared->version = input->version;
                        prepared->input =
                            std::make_shared<const CollectionInput>(std::move(*input));
                        prepared->tokens = tokens->snapshot;
                        prepared->expanded = prepared->input->expanded
                                                 ? prepared->input->expanded(prepared->tokens)
                                                 : std::vector<CollectionToken>{};
                    } else {
                        const auto expansion = source->read_expansion(dataset->tokens);
                        if (!expansion)
                            continue;
                        prepared->version = expansion->version;
                        prepared->expanded = expansion->expanded;
                        prepared->input = dataset->input;
                        prepared->tokens = dataset->tokens;
                        prepared->graph = dataset->graph;
                    }
                    if (hierarchy.reject_sections &&
                        std::find(prepared->input->sections.begin(),
                                  prepared->input->sections.end(),
                                  true) != prepared->input->sections.end())
                        throw CollectionModelError("GridView does not accept section headers");
                    if (hierarchy.hierarchical) {
                        if (!prepared->input->parents)
                            throw std::invalid_argument("hierarchical collection requires parents");
                        if (rows_changed)
                            prepared->graph = std::make_shared<const CollectionTreeGraph>(
                                prepare_collection_tree_graph(
                                    prepared->input->parents(prepared->tokens),
                                    prepared->input->branches));
                        std::vector<bool> expanded(prepared->tokens->tokens.size(), false);
                        for (const auto token : prepared->expanded)
                            if (const auto i =
                                    CollectionTokens::index_for_token(*prepared->tokens, token))
                                expanded[*i] = true;
                        for (const auto &row : flatten_collection_tree(*prepared->graph, expanded))
                            prepared->visible.push_back(
                                {row.index, row.depth, row.branch, row.expanded});
                    } else {
                        prepared->visible.reserve(prepared->input->rows.size());
                        for (std::size_t index = 0; index < prepared->input->rows.size(); ++index)
                            prepared->visible.push_back({index, 0, false, false});
                    }
                    if (options.empty_content)
                        prepared->visible.clear();
                    VirtualSemanticChildren::Metadata metadata;
                    metadata.reserve(prepared->visible.size());
                    std::vector<double> next_heights;
                    next_heights.reserve(prepared->visible.size());
                    for (std::size_t visible = 0; visible < prepared->visible.size(); ++visible) {
                        const auto &row = prepared->visible[visible];
                        const auto index = row.dataset_index;
                        const auto token = prepared->tokens->tokens[index];
                        const auto &item = prepared->input->rows[index];
                        prepared->order.push_back(token);
                        prepared->eligible.push_back(item.eligible);
                        prepared->search.push_back(item);
                        prepared->visible_index.emplace(token, visible);
                        if (!std::isfinite(static_cast<double>(row.depth) * hierarchy.indentation))
                            throw std::overflow_error("collection indentation overflow");
                        auto payload = next_payload++;
                        double height = options.row_heights.estimate;
                        if (dataset)
                            if (const auto old_index = dataset_index(token)) {
                                const auto parent_token = [&](const CollectionDataset &model,
                                                              std::size_t i) {
                                    return hierarchy.hierarchical && model.graph->parents[i]
                                               ? model.tokens->tokens[*model.graph->parents[i]]
                                               : CollectionToken{};
                                };
                                const bool same =
                                    dataset->input->rows[*old_index].label == item.label &&
                                    dataset->input->rows[*old_index].eligible == item.eligible &&
                                    dataset->input->sections[*old_index] ==
                                        prepared->input->sections[index] &&
                                    dataset->input->branches[*old_index] ==
                                        prepared->input->branches[index] &&
                                    parent_token(*dataset, *old_index) ==
                                        parent_token(*prepared, index);
                                if (same)
                                    if (const auto old_payload = dataset->payloads.find(token);
                                        old_payload != dataset->payloads.end())
                                        payload = old_payload->second;
                                if (const auto old_visible = visible_index(token))
                                    height = heights.height_at(*old_visible);
                            }
                        prepared->payloads.emplace(token, payload);
                        next_heights.push_back(height);
                        VirtualSemanticItemMetadata semantic;
                        semantic.token = token;
                        semantic.name = item.label;
                        semantic.enabled = item.eligible;
                        semantic.role =
                            hierarchy.hierarchical ? SemanticRole::Custom : SemanticRole::ListItem;
                        if (row.branch)
                            semantic.expanded = row.expanded ? SemanticExpandedState::Expanded
                                                             : SemanticExpandedState::Collapsed;
                        semantic.level = row.depth;
                        if (hierarchy.hierarchical && prepared->graph->parents[index])
                            semantic.parent_token =
                                prepared->tokens->tokens[*prepared->graph->parents[index]];
                        semantic.actions = {SemanticAction::Focus};
                        if (item.eligible) {
                            semantic.actions.push_back(SemanticAction::Select);
                            semantic.actions.push_back(SemanticAction::Activate);
                            if (row.branch)
                                semantic.actions.push_back(row.expanded ? SemanticAction::Collapse
                                                                        : SemanticAction::Expand);
                        }
                        metadata.push_back(std::move(semantic));
                    }
                    prepared->metadata = std::make_shared<const VirtualSemanticChildren::Metadata>(
                        std::move(metadata));
                    CollectionHeightIndex prepared_heights;
                    prepared_heights.replace(std::move(next_heights));
                    if (source->version() != prepared->version)
                        continue;
                    // Preserve the top visible key and intra-row inset through a
                    // reorder/expansion. Model publication precedes no callbacks.
                    const auto previous = dataset;
                    double anchor_inset = 0.0;
                    std::optional<CollectionToken> anchor;
                    std::optional<std::size_t> anchor_index;
                    if (dataset)
                        if (const auto top = heights.index_at(offset.y)) {
                            anchor = dataset->order[*top];
                            anchor_index = top;
                            anchor_inset = offset.y - heights.prefix_sum(*top);
                        }
                    if (tokens && !token_registry.commit(std::move(*tokens)))
                        continue;
                    dataset = std::move(prepared);
                    heights = std::move(prepared_heights);
                    ++epoch;
                    rejected.reset();
                    selection_dirty = rows_changed;
                    if (anchor && previous) {
                        auto surviving = visible_index(*anchor);
                        if (!surviving && hierarchy.hierarchical)
                            if (const auto old_index =
                                    CollectionTokens::index_for_token(*previous->tokens, *anchor)) {
                                auto parent = previous->graph->parents[*old_index];
                                while (parent && !surviving) {
                                    surviving = visible_index(previous->tokens->tokens[*parent]);
                                    parent = previous->graph->parents[*parent];
                                }
                            }
                        if (!surviving && anchor_index)
                            for (std::size_t i = *anchor_index;
                                 i < previous->order.size() && !surviving; ++i)
                                surviving = visible_index(previous->order[i]);
                        if (!surviving && anchor_index)
                            for (std::size_t i = *anchor_index; i > 0 && !surviving; --i)
                                surviving = visible_index(previous->order[i - 1]);
                        if (surviving)
                            offset.y =
                                extent(heights.prefix_sum(*surviving) +
                                       std::min(anchor_inset, heights.height_at(*surviving)));
                    }
                    if (contact->pressed)
                        stop_contact(contact);
                    pending |= 15;
                    break;
                } catch (const CollectionModelError &) {
                    if (source->version() != attempted_version)
                        continue;
                    if (!dataset || initial)
                        throw;
                    rejected = attempted_version;
                    break;
                }
            }
            if (!dataset)
                throw std::runtime_error("collection snapshot did not stabilize");
        }
        if (dataset &&
            (selection_revision != source->selection_revision() || initial || selection_dirty)) {
            if (const auto current = source->read_selection(dataset->tokens)) {
                if (selection != current->selection) {
                    auto selected = std::make_shared<std::vector<VirtualSemanticItemToken>>(
                        current->selection.selected);
                    std::sort(selected->begin(), selected->end());
                    selected->erase(std::unique(selected->begin(), selected->end()),
                                    selected->end());
                    auto next = current->selection;
                    selection = std::move(next);
                    selected_tokens = std::move(selected);
                    contact->active = selection.active;
                    pending |= 8;
                }
                selection_revision = current->revision;
                selection_dirty = false;
            }
        }
        recover_active();
        prepare_semantics();
        if (published_dataset == dataset)
            published_selected = selected_tokens;
        if (!source->valid() || (contact->disclosure && !source->expansion_valid()) ||
            (!contact->disclosure && !source->selection_valid()))
            stop_contact_noexcept(contact);
        preparing_models = false;
        flush_effects();
    }
    void recover_active() noexcept {
        if (!dataset || !contact->active || visible_index(*contact->active))
            return;
        if (hierarchy.hierarchical)
            if (const auto index = dataset_index(*contact->active)) {
                auto current = dataset->graph->parents[*index];
                while (current) {
                    const auto token = dataset->tokens->tokens[*current];
                    if (visible_index(token)) {
                        contact->active = token;
                        return;
                    }
                    current = dataset->graph->parents[*current];
                }
            }
        contact->active.reset();
        for (std::size_t i = 0; i < dataset->order.size(); ++i)
            if (dataset->eligible[i]) {
                contact->active = dataset->order[i];
                break;
            }
    }
    [[nodiscard]] CollectionLayoutSnapshot arrangement(Rect bounds) const {
        auto geometry_bounds = bounds;
        auto geometry_offset = offset;
        if (columns_geometry) {
            geometry_bounds.w = extent(std::max<double>(bounds.w, columns_geometry->total_width()));
            geometry_offset.x = coordinate(columns_geometry->horizontal_offset());
        }
        auto prepared = layout->prepare(heights.snapshot(), dataset->visible.size(),
                                        geometry_bounds, geometry_offset, hierarchy.virtualized);
        // Active/captured rows may remain outside the viewport without causing
        // every selected row in a million-item model to be materialized.
        const auto add = [&](std::optional<CollectionToken> token) {
            if (token)
                if (const auto i = visible_index(*token))
                    prepared.window.push_back(*i);
        };
        if (contact->focused)
            add(contact->active);
        add(contact->pressed);
        std::sort(prepared.window.begin(), prepared.window.end());
        prepared.window.erase(std::unique(prepared.window.begin(), prepared.window.end()),
                              prepared.window.end());
        return prepared;
    }
    [[nodiscard]] std::vector<std::string> keys() {
        sync();
        const auto plan = arrangement(viewport);
        std::vector<std::string> result;
        result.reserve(plan.window.size());
        const auto current = dataset;
        for (const auto visible : plan.window) {
            const auto token = current->order[visible];
            result.push_back(row_key(token, current->payloads.at(token)));
        }
        return result;
    }
    [[nodiscard]] std::vector<Spec> build_window() {
        sync();
        const auto owner = shared_from_this();
        const auto current = dataset;
        const auto current_epoch = epoch;
        const auto plan = arrangement(viewport);
        const auto before = source->version();
        std::map<std::string, Spec> prepared;
        std::vector<Spec> result;
        result.reserve(plan.window.size());
        for (const auto visible : plan.window) {
            const auto token = current->order[visible];
            const auto key = row_key(token, current->payloads.at(token));
            Spec row;
            if (const auto cached = built.find(key); cached != built.end())
                row = cached->second;
            else {
                auto child =
                    current->input->make_row(current->visible[visible].dataset_index, columns);
                if (epoch != current_epoch || source->version() != before)
                    throw std::runtime_error("collection changed during row factory");
                const std::weak_ptr<CollectionRuntime> weak = owner;
                row.factory = [weak, token] {
                    const auto runtime = weak.lock();
                    if (!runtime)
                        throw std::logic_error("collection row session has expired");
                    return std::make_unique<CollectionRow>(runtime, token);
                };
                row.children.push_back(std::move(child));
            }
            prepared.emplace(key, row);
            result.push_back(std::move(row));
            if (epoch != current_epoch || source->version() != before)
                throw std::runtime_error("collection changed during row copy");
        }
        built.swap(prepared);
        retained_window = plan.window;
        return result;
    }
    [[nodiscard]] std::optional<std::size_t> hit(Point point) const noexcept {
        if (!published.geometry || !viewport.contains(point))
            return {};
        // Window-sized only, using exact committed geometry. Gap/empty cells
        // cannot hit a neighbouring row via a coarse index approximation.
        for (const auto i : published.window)
            if (published.geometry->bounds_at(i).contains(point))
                return i;
        return {};
    }
    void ensure_visible(CollectionToken token) {
        const auto index = visible_index(token);
        if (!index)
            return;
        auto plan = arrangement(viewport);
        const auto rect = plan.geometry->bounds_at(*index);
        const double top = rect.y - viewport.y + offset.y;
        double next = offset.y;
        if (rect.y < viewport.y)
            next = top;
        else if (static_cast<double>(rect.y) + rect.h >
                 static_cast<double>(viewport.y) + viewport.h)
            next = top + rect.h - viewport.h;
        next =
            std::clamp(next, 0.0, std::max(0.0, static_cast<double>(plan.content.h) - viewport.h));
        if (next != offset.y) {
            offset.y = extent(next);
            mark(3);
        }
    }
    void choose(CollectionToken token, CollectionSelectionGesture gesture,
                const std::function<bool()> &input_permission) {
        if (!writable() || !eligible(token))
            return;
        const auto owner = shared_from_this();
        const auto current = dataset;
        const auto serial = contact->serial, generation = epoch;
        const auto source_before = source->version();
        const auto selection_before = source->selection_revision();
        auto candidate_selection = resolve_collection_selection(
            current->order, current->eligible, selection,
            options.selection_mode == SelectionMode::Multiple, token, gesture);
        if (candidate_selection == selection) {
            ensure_visible(token);
            return;
        }
        const auto allowed = guard(serial, generation, input_permission);
        const auto invalidate = invalidate_layout;
        if (invalidate)
            invalidate();
        if (!allowed() || source->version() != source_before ||
            source->selection_revision() != selection_before)
            return;
        (void)source->publish_selection(current->tokens, std::move(candidate_selection), allowed);
        if (allowed()) {
            sync();
            ensure_visible(token);
        }
    }
    void toggle(CollectionToken token, bool recursive,
                const std::function<bool()> &input_permission) {
        if (!mutable_available() || !source->expansion_valid())
            return;
        const auto index = visible_index(token);
        if (!index || !dataset->visible[*index].branch)
            return;
        const auto owner = shared_from_this();
        const auto current = dataset;
        const auto serial = contact->serial, generation = epoch;
        const bool open = current->visible[*index].expanded;
        const auto source_before = source->version();
        const auto selection_before = source->selection_revision();
        std::vector<bool> flags(current->tokens->tokens.size(), false);
        for (const auto expanded : current->expanded)
            if (const auto i = CollectionTokens::index_for_token(*current->tokens, expanded))
                flags[*i] = true;
        std::vector<std::size_t> pending_indices{current->visible[*index].dataset_index};
        for (std::size_t pos = 0; pos < pending_indices.size(); ++pos) {
            const auto node = pending_indices[pos];
            if (current->graph->branches[node])
                flags[node] = !open;
            if (recursive)
                for (const auto child : current->graph->children[node])
                    pending_indices.push_back(child);
        }
        std::vector<CollectionToken> expanded;
        for (std::size_t i = 0; i < flags.size(); ++i)
            if (flags[i])
                expanded.push_back(current->tokens->tokens[i]);
        const auto allowed = guard(serial, generation, input_permission);
        const auto invalidate = invalidate_layout;
        if (invalidate)
            invalidate();
        if (!allowed() || source->version() != source_before ||
            source->selection_revision() != selection_before)
            return;
        const bool committed =
            source->publish_expanded(current->tokens, std::move(expanded), allowed);
        if (!committed || !contact->mounted || !source->valid() ||
            source->version().rows != source_before.rows ||
            source->version().expansion != source_before.expansion + 1 ||
            source->selection_revision() != selection_before)
            return;
        sync();
        if (open && contact->mounted && writable()) {
            // A user collapse canonises only the gesture's own selection. An
            // external collapse never rewrites the application model.
            if (selection.active && !visible_index(*selection.active)) {
                if (options.selection_mode == SelectionMode::Single)
                    choose(token, CollectionSelectionGesture::Replace, input_permission);
                else {
                    auto canonical = selection;
                    canonical.selected.erase(std::remove_if(canonical.selected.begin(),
                                                            canonical.selected.end(),
                                                            [&](const auto selected) {
                                                                return !visible_index(selected);
                                                            }),
                                             canonical.selected.end());
                    canonical.active = token;
                    if (canonical.anchor && !visible_index(*canonical.anchor))
                        canonical.anchor = token;
                    const auto permitted = guard(contact->serial, epoch, input_permission);
                    (void)source->publish_selection(dataset->tokens, std::move(canonical),
                                                    permitted);
                    if (permitted())
                        sync();
                }
            }
        }
    }
    [[nodiscard]] bool attached() const noexcept override { return contact->mounted; }
    [[nodiscard]] std::shared_ptr<const CollectionTokenSnapshot>
    token_snapshot() const noexcept override {
        return dataset->tokens;
    }
    [[nodiscard]] std::vector<CollectionToken> visible_tokens() const override {
        return dataset->order;
    }
    [[nodiscard]] std::optional<std::size_t>
    depth_for_token(CollectionToken token) const noexcept override {
        const auto index = dataset_index(token);
        if (!index)
            return {};
        std::size_t depth = 0;
        if (hierarchy.hierarchical) {
            auto parent = dataset->graph->parents[*index];
            while (parent) {
                ++depth;
                parent = dataset->graph->parents[*parent];
            }
        }
        return depth;
    }
    [[nodiscard]] CollectionRange visible_range() const noexcept override {
        if (!published.geometry)
            return {};
        CollectionRange range{published_dataset ? published_dataset->order.size() : 0, 0};
        for (const auto index : published.window)
            if (!intersect(published.geometry->bounds_at(index), viewport).empty()) {
                range.first = std::min(range.first, index);
                range.last = std::max(range.last, index + 1);
            }
        return range.last ? range : CollectionRange{};
    }
    [[nodiscard]] VirtualSemanticChildren semantic_children() const override {
        if (!published_dataset || !published.geometry)
            return {};
        return VirtualSemanticChildren::from_geometry(published_epoch, published_metadata,
                                                      published_selected, published.geometry);
    }
    bool scroll_to(CollectionToken token, ScrollAlignment alignment) override {
        const auto owner = shared_from_this();
        if (!contact->mounted || !source->valid())
            return false;
        sync();
        const auto index = visible_index(token);
        if (!index)
            return false;
        const auto plan = arrangement(viewport);
        const auto rect = plan.geometry->bounds_at(*index);
        const double top = static_cast<double>(rect.y) - viewport.y + offset.y;
        double next = offset.y;
        switch (alignment) {
        case ScrollAlignment::Nearest:
            if (rect.y < viewport.y)
                next = top;
            else if (static_cast<double>(rect.y) + rect.h >
                     static_cast<double>(viewport.y) + viewport.h)
                next = top + rect.h - viewport.h;
            break;
        case ScrollAlignment::Start:
            next = top;
            break;
        case ScrollAlignment::Center:
            next = top + (rect.h - viewport.h) / 2.0;
            break;
        case ScrollAlignment::End:
            next = top + rect.h - viewport.h;
            break;
        }
        next =
            std::clamp(next, 0.0, std::max(0.0, static_cast<double>(plan.content.h) - viewport.h));
        if (next != offset.y) {
            offset.y = extent(next);
            mark(3);
        }
        return contact->mounted;
    }
};
ComponentAvailability CollectionRow::local_availability() const noexcept {
    const auto index = runtime_->dataset_index(token_);
    return {index ? VisibilityMode::Visible : VisibilityMode::Collapsed,
            index && runtime_->dataset->input->rows[*index].eligible, false};
}
Size CollectionRow::measure(const std::vector<ChildMetrics> &children) const {
    const auto index = runtime_->visible_index(token_);
    const double indent =
        index && !runtime_->columns_geometry
            ? runtime_->dataset->visible[*index].depth * runtime_->hierarchy.indentation +
                  (runtime_->hierarchy.hierarchical ? runtime_->hierarchy.chevron_width : 0.0)
            : 0.0;
    const auto child = children.empty() ? Size{} : children.front().preferred;
    const auto style = runtime_->row_style(token_);
    return {extent(child.w + indent + 2 * nonnegative(style.row_horizontal_inset)),
            extent(std::max<double>(runtime_->options.row_heights.estimate,
                                    child.h + 2 * nonnegative(style.row_vertical_inset)))};
}
Size CollectionRow::minimum_size(const std::vector<ChildMetrics> &) const {
    return {0, extent(runtime_->options.row_heights.estimate)};
}
Constraints CollectionRow::child_constraints(const Constraints &constraints, std::size_t,
                                             std::size_t) const {
    const auto index = runtime_->visible_index(token_);
    const double indent =
        index && !runtime_->columns_geometry
            ? runtime_->dataset->visible[*index].depth * runtime_->hierarchy.indentation +
                  (runtime_->hierarchy.hierarchical ? runtime_->hierarchy.chevron_width : 0.0)
            : 0.0;
    const auto style = runtime_->row_style(token_);
    return {
        {0, 0},
        {std::isfinite(constraints.max.w)
             ? extent(
                   constraints.max.w - indent -
                   (runtime_->columns_geometry ? 0.0 : 2 * nonnegative(style.row_horizontal_inset)))
             : constraints.max.w,
         kUnboundedExtent}};
}
void CollectionRow::layout_children(Rect bounds, const std::vector<ChildMetrics> &,
                                    std::vector<ChildPlacement> &placements) const {
    if (placements.empty())
        return;
    const auto index = runtime_->visible_index(token_);
    const double indent =
        index && !runtime_->columns_geometry
            ? runtime_->dataset->visible[*index].depth * runtime_->hierarchy.indentation +
                  (runtime_->hierarchy.hierarchical ? runtime_->hierarchy.chevron_width : 0.0)
            : 0.0;
    if (runtime_->columns_geometry)
        placements.front().bounds = bounds;
    else {
        const auto style = runtime_->row_style(token_);
        const double horizontal = nonnegative(style.row_horizontal_inset),
                     vertical = nonnegative(style.row_vertical_inset);
        placements.front().bounds = {
            coordinate(bounds.x + indent + horizontal), coordinate(bounds.y + vertical),
            extent(bounds.w - indent - 2 * horizontal), extent(bounds.h - 2 * vertical)};
    }
}
void CollectionRow::bind_descendant_context(Component &component) const {
    if (auto *participant = dynamic_cast<CollectionColumnsParticipant *>(&component)) {
        if (runtime_->columns_geometry)
            participant->bind_collection_columns(runtime_->columns_geometry);
        const auto index = runtime_->visible_index(token_);
        const double indent =
            index ? runtime_->dataset->visible[*index].depth * runtime_->hierarchy.indentation +
                        (runtime_->hierarchy.hierarchical ? runtime_->hierarchy.chevron_width : 0.0)
                  : 0.0;
        participant->bind_collection_row_indent(indent);
        const auto style = runtime_->row_style(token_);
        participant->bind_collection_row_padding(nonnegative(style.row_horizontal_inset),
                                                 nonnegative(style.row_vertical_inset));
    }
}
SemanticInfo CollectionRow::semantics() const {
    SemanticInfo info;
    info.role = runtime_->hierarchy.hierarchical ? SemanticRole::Custom : SemanticRole::ListItem;
    if (const auto index = runtime_->dataset_index(token_))
        info.name = runtime_->dataset->input->rows[*index].label;
    info.enabled = effective_enabled();
    info.read_only = effective_read_only() || !runtime_->source->valid();
    info.selected = std::binary_search(runtime_->selected_tokens->begin(),
                                       runtime_->selected_tokens->end(), token_);
    if (const auto index = runtime_->visible_index(token_);
        index && runtime_->dataset->visible[*index].branch)
        info.expanded = runtime_->dataset->visible[*index].expanded
                            ? SemanticExpandedState::Expanded
                            : SemanticExpandedState::Collapsed;
    return info;
}
class CollectionViewComponent final : public Component,
                                      public DynamicChildrenSource,
                                      public ThemeBinding,
                                      public CollectionColumnsParticipant {
  public:
    explicit CollectionViewComponent(std::shared_ptr<CollectionRuntime> runtime)
        : runtime_(std::move(runtime)) {}
    void bind_collection_columns(std::shared_ptr<CollectionColumnsGeometry> layout) override {
        runtime_->columns_geometry = std::move(layout);
    }
    void bind_theme(const Theme &theme) noexcept override {
        ThemeBinding::bind_theme(theme);
        runtime_->theme = &theme;
    }
    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }
    [[nodiscard]] bool clips_children() const noexcept override { return true; }
    [[nodiscard]] bool uses_retained_checkpoint() const noexcept override { return true; }
    [[nodiscard]] std::vector<std::string> desired_keys() const override {
        return runtime_->keys();
    }
    [[nodiscard]] std::vector<DynamicChildSpec> desired_children() const override {
        auto children = runtime_->build_window();
        const auto current = runtime_->dataset;
        std::vector<DynamicChildSpec> result;
        result.reserve(children.size());
        for (std::size_t i = 0; i < children.size(); ++i) {
            const auto token = current->order[runtime_->retained_window[i]];
            result.push_back({row_key(token, current->payloads.at(token)), std::move(children[i])});
        }
        return result;
    }
    void set_structure_invalidator(std::function<void()> callback) override {
        runtime_->invalidate_structure = std::move(callback);
    }
    [[nodiscard]] std::vector<Spec> initial_children() { return runtime_->build_window(); }
    [[nodiscard]] Size measure(const std::vector<ChildMetrics> &children) const override {
        const auto runtime = runtime_;
        float width = 0;
        for (std::size_t i = 0; i < children.size() && i < runtime->retained_window.size(); ++i) {
            width = std::max(width, children[i].preferred.w);
            if (runtime->options.row_heights.variable && children[i].participates_in_layout)
                (void)runtime->heights.set_height(
                    runtime->retained_window[i],
                    std::max<double>(runtime->options.row_heights.estimate,
                                     children[i].preferred.h));
        }
        return {std::max(width, runtime->viewport.w), extent(runtime->heights.total_height())};
    }
    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics> &) const override { return {}; }
    [[nodiscard]] Constraints child_constraints(const Constraints &constraints, std::size_t index,
                                                std::size_t) const override {
        const auto runtime = runtime_;
        Rect viewport = runtime->viewport;
        if (constraints.bounded_width())
            viewport.w = constraints.max.w;
        const auto plan = runtime->arrangement(viewport);
        const auto width = index < runtime->retained_window.size()
                               ? plan.geometry->bounds_at(runtime->retained_window[index]).w
                               : viewport.w;
        return {{0, 0}, {width, kUnboundedExtent}};
    }
    void layout_children(Rect bounds, const std::vector<ChildMetrics> &,
                         std::vector<ChildPlacement> &placements) const override {
        const auto runtime = runtime_;
        runtime->candidate_offset = runtime->offset;
        auto geometry_bounds = bounds;
        if (runtime->columns_geometry) {
            geometry_bounds.w =
                extent(std::max<double>(bounds.w, runtime->columns_geometry->total_width()));
            runtime->candidate_offset.x =
                coordinate(runtime->columns_geometry->horizontal_offset());
        }
        // Reflow preserves the first physically visible key and its inset. A
        // pending explicit scroll has priority over this automatic anchor.
        if (runtime->published.geometry && runtime->published_dataset &&
            runtime->offset.y == runtime->published_offset.y) {
            for (const auto old_index : runtime->published.window) {
                const auto old_rect = runtime->published.geometry->bounds_at(old_index);
                if (intersect(old_rect, runtime->viewport).empty())
                    continue;
                const auto token = runtime->published_dataset->order[old_index];
                if (const auto index = runtime->visible_index(token)) {
                    const auto zero = runtime->layout->prepare(
                        runtime->heights.snapshot(), runtime->dataset->order.size(),
                        geometry_bounds, {}, runtime->hierarchy.virtualized);
                    const auto rect = zero.geometry->bounds_at(*index);
                    runtime->candidate_offset.y = extent(
                        static_cast<double>(rect.y) - bounds.y +
                        std::max(0.0, static_cast<double>(runtime->viewport.y) - old_rect.y));
                }
                break;
            }
        }
        auto candidate = runtime->layout->prepare(
            runtime->heights.snapshot(), runtime->dataset->order.size(), geometry_bounds,
            runtime->candidate_offset, runtime->hierarchy.virtualized);
        runtime->candidate_offset.y = extent(
            std::min<double>(runtime->candidate_offset.y,
                             std::max(0.0, static_cast<double>(candidate.content.h) - bounds.h)));
        candidate = runtime->layout->prepare(
            runtime->heights.snapshot(), runtime->dataset->order.size(), geometry_bounds,
            runtime->candidate_offset, runtime->hierarchy.virtualized);
        runtime->candidate = std::move(candidate);
        runtime->candidate_dataset = runtime->dataset;
        runtime->candidate_selected = runtime->selected_tokens;
        runtime->candidate_metadata = runtime->semantic_metadata;
        runtime->candidate_epoch = runtime->epoch;
        for (std::size_t i = 0; i < placements.size(); ++i)
            placements[i].bounds =
                i < runtime->retained_window.size()
                    ? runtime->candidate.geometry->bounds_at(runtime->retained_window[i])
                    : Rect{};
    }
    void mount(MountContext &context) override {
        const auto runtime = runtime_;
        runtime->contact->mounted = true;
        runtime->mount_permission = InputMutationAccess::guard(context);
        runtime->invalidate_layout = context.layout_invalidator();
        runtime->invalidate_paint = context.invalidator();
        runtime->invalidate_availability = context.availability_invalidator();
        runtime->request_focus = context.focus_requester();
        runtime->theme = &current_theme();
        runtime->availability = effective_availability();
        const std::weak_ptr<CollectionRuntime> weak = runtime;
        runtime->subscription = runtime->source->observe([weak] {
            if (const auto current = weak.lock())
                current->mark(15);
        });
        if (runtime->options.controller)
            runtime->options.controller->attach(runtime);
        runtime->sync();
    }
    void unmount(LifecycleContext &) override {
        const auto runtime = runtime_;
        if (runtime->options.controller)
            runtime->options.controller->detach(runtime.get());
        runtime->contact->mounted = false;
        stop_contact_noexcept(runtime->contact);
        runtime->clear_typeahead();
        runtime->subscription.reset();
        runtime->invalidate_layout = {};
        runtime->invalidate_paint = {};
        runtime->invalidate_availability = {};
        runtime->request_focus = {};
        runtime->mount_permission = {};
        runtime->pending = 0;
    }
    void activate(LifecycleContext &context) override {
        runtime_->dispatcher = context.dispatcher();
    }
    void deactivate(LifecycleContext &) override {
        stop_contact_noexcept(runtime_->contact);
        runtime_->contact->hovered.reset();
        runtime_->clear_typeahead();
        runtime_->dispatcher = {};
    }
    void focus_changed(bool focused, FocusContext &context) override {
        const auto runtime = runtime_;
        runtime->contact->focused = focused;
        if (!focused)
            runtime->clear_typeahead();
        else if (!runtime->contact->active || !runtime->visible_index(*runtime->contact->active)) {
            runtime->contact->active = runtime->selection.active;
            if (!runtime->contact->active || !runtime->visible_index(*runtime->contact->active))
                for (std::size_t i = 0; i < runtime->dataset->order.size(); ++i)
                    if (runtime->dataset->eligible[i]) {
                        runtime->contact->active = runtime->dataset->order[i];
                        break;
                    }
        }
        context.invalidate();
    }
    [[nodiscard]] SemanticInfo semantics() const override {
        SemanticInfo info;
        info.role = runtime_->hierarchy.hierarchical ? SemanticRole::Group : SemanticRole::ListView;
        info.enabled = effective_enabled();
        info.read_only = effective_read_only() || !runtime_->source->valid();
        info.focusable = true;
        info.focused = runtime_->contact->focused;
        if (info.enabled)
            info.actions.push_back(SemanticAction::Focus);
        return info;
    }
    EventResult input(const InputEvent &event, InputContext &context) override {
        const auto runtime = runtime_;
        const auto contact = runtime->contact;
        const auto input_permission = InputMutationAccess::guard(context);
        if (event.type == InputType::PointerCancel) {
            const bool armed = contact->pressed.has_value();
            runtime->stop_contact_presented();
            return armed ? EventResult::Handled : EventResult::Ignored;
        }
        if (!effective_enabled())
            return EventResult::Ignored;
        runtime->availability = effective_availability();
        if (event.type == InputType::PointerWheel) {
            const bool horizontal =
                runtime->columns_geometry &&
                runtime->columns_geometry->set_horizontal_offset(
                    runtime->columns_geometry->published_horizontal_offset() - event.delta.x);
            const auto plan = runtime->arrangement(runtime->viewport);
            const double next = std::clamp(
                static_cast<double>(runtime->offset.y) - event.delta.y, 0.0,
                std::max(0.0, static_cast<double>(plan.content.h) - runtime->viewport.h));
            if (next == runtime->offset.y && !horizontal)
                return EventResult::Ignored;
            runtime->offset.y = extent(next);
            runtime->mark(3);
            return EventResult::Handled;
        }
        if (event.type == InputType::PointerLeave) {
            runtime->update_hover({});
            return EventResult::Ignored;
        }
        const auto hit = runtime->hit(event.position);
        if (event.type == InputType::PointerMove) {
            const auto move_serial = contact->serial, move_generation = runtime->epoch;
            const auto next =
                hit ? std::optional<CollectionToken>{runtime->dataset->order[*hit]} : std::nullopt;
            runtime->update_hover(next);
            if (!contact->mounted || contact->serial != move_serial ||
                runtime->epoch != move_generation)
                return EventResult::Ignored;
            if (runtime->options.reorder && contact->pressed && runtime->writable() &&
                !contact->disclosure) {
                const double dx = static_cast<double>(event.position.x) - contact->down.x;
                const double dy = static_cast<double>(event.position.y) - contact->down.y;
                if (!contact->dragging && dx * dx + dy * dy >= 16.0) {
                    contact->dragging = true;
                    if (std::find(runtime->selection.selected.begin(),
                                  runtime->selection.selected.end(),
                                  *contact->pressed) == runtime->selection.selected.end())
                        runtime->choose(*contact->pressed, CollectionSelectionGesture::Replace,
                                        input_permission);
                }
                if (contact->dragging && runtime->writable()) {
                    contact->insertion = next;
                    const auto plan = runtime->arrangement(runtime->viewport);
                    double delta = 0;
                    if (event.position.y < runtime->viewport.y + 16)
                        delta = -12;
                    else if (event.position.y > runtime->viewport.y + runtime->viewport.h - 16)
                        delta = 12;
                    const double offset = std::clamp(
                        static_cast<double>(runtime->offset.y) + delta, 0.0,
                        std::max(0.0, static_cast<double>(plan.content.h) - runtime->viewport.h));
                    if (offset != runtime->offset.y) {
                        runtime->offset.y = extent(offset);
                        runtime->mark(3);
                    } else
                        context.invalidate();
                }
            }
            return contact->pressed ? EventResult::Handled : EventResult::Ignored;
        }
        if (event.type == InputType::PointerDown) {
            if (!hit || !runtime->mutable_available())
                return EventResult::Ignored;
            const auto token = runtime->dataset->order[*hit];
            if (!runtime->eligible(token))
                return EventResult::Ignored;
            auto release = context.pointer_releaser();
            const auto focus = runtime->request_focus;
            stop_contact(contact);
            if (!runtime->mutable_available())
                return EventResult::Ignored;
            const auto before_style = runtime->row_style(token);
            contact->pressed = token;
            contact->down = event.position;
            contact->release = std::move(release);
            const auto &row = runtime->dataset->visible[*hit];
            const auto rect = runtime->published.geometry->bounds_at(*hit);
            auto disclosure_rect = rect;
            if (runtime->columns_geometry && !runtime->columns_geometry->hierarchy_column().empty())
                disclosure_rect = runtime->columns_geometry->published_column_bounds(
                    std::string(runtime->columns_geometry->hierarchy_column()), rect);
            const double chevron_x = disclosure_rect.x + row.depth * runtime->hierarchy.indentation;
            const Rect chevron{
                coordinate(chevron_x), disclosure_rect.y,
                extent(std::min(runtime->hierarchy.chevron_width,
                                std::max(0.0, static_cast<double>(disclosure_rect.x) +
                                                  disclosure_rect.w - chevron_x))),
                disclosure_rect.h};
            contact->disclosure = row.branch && chevron.contains(event.position);
            const auto serial = contact->serial, generation = runtime->epoch;
            if (!contact->focused && focus)
                focus();
            if (!runtime->mutable_available() || contact->serial != serial ||
                runtime->epoch != generation) {
                stop_contact(contact);
                return EventResult::Handled;
            }
            context.capture_pointer();
            runtime->changed_presentation(token, before_style);
            return EventResult::Handled;
        }
        if (event.type == InputType::PointerUp) {
            const auto pressed = contact->pressed;
            const bool disclosure = contact->disclosure;
            const bool dragging = contact->dragging;
            const auto insertion = contact->insertion;
            const auto current_dataset = runtime->dataset;
            const auto generation = runtime->epoch;
            const bool same = pressed && hit && current_dataset->order[*hit] == *pressed;
            const auto terminal_serial = contact->serial + 1;
            runtime->stop_contact_presented();
            if (!contact->mounted || contact->serial != terminal_serial ||
                runtime->epoch != generation)
                return pressed ? EventResult::Handled : EventResult::Ignored;
            if (dragging) {
                const auto selected = runtime->selection.selected;
                if (runtime->writable() && runtime->epoch == generation)
                    runtime->source->reorder(current_dataset->tokens, selected, insertion,
                                             runtime->guard(contact->serial, generation,
                                                            input_permission));
                return EventResult::Handled;
            }
            if (!same || !runtime->mutable_available())
                return pressed ? EventResult::Handled : EventResult::Ignored;
            const auto token = *pressed;
            contact->active = token;
            const auto serial = contact->serial;
            if (disclosure)
                runtime->toggle(token, event.alt, input_permission);
            else {
                const auto gesture = event.shift
                                         ? (event.primary ? CollectionSelectionGesture::AddRange
                                                          : CollectionSelectionGesture::Extend)
                                     : event.primary ? CollectionSelectionGesture::Toggle
                                                     : CollectionSelectionGesture::Replace;
                runtime->choose(token, gesture, input_permission);
                if (event.clicks > 1 && runtime->mutable_available() && runtime->eligible(token) &&
                    contact->serial == serial && runtime->epoch == generation)
                    runtime->source->activate(current_dataset->tokens, token,
                                              runtime->guard(serial, generation,
                                                             input_permission));
            }
            return EventResult::Handled;
        }
        if (event.type == InputType::TextInput && runtime->options.typeahead) {
            const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch());
            const auto active =
                contact->active ? runtime->visible_index(*contact->active) : std::nullopt;
            const auto target =
                runtime->typeahead.feed(event.text, now, runtime->dataset->search, active);
            runtime->expire_typeahead();
            if (target) {
                contact->active = runtime->dataset->order[*target];
                if (runtime->writable())
                    runtime->choose(runtime->dataset->order[*target],
                                    CollectionSelectionGesture::Replace, input_permission);
                else
                    runtime->ensure_visible(runtime->dataset->order[*target]);
            }
            return EventResult::Handled;
        }
        if (event.type != InputType::KeyDown)
            return EventResult::Ignored;
        if (event.key == Key::Escape) {
            runtime->stop_contact_presented();
            runtime->clear_typeahead();
            return EventResult::Handled;
        }
        const auto dataset = runtime->dataset;
        const auto count = dataset->order.size();
        if (!count)
            return EventResult::Ignored;
        auto active = contact->active ? runtime->visible_index(*contact->active) : std::nullopt;
        if (!active) {
            for (std::size_t i = 0; i < count; ++i)
                if (dataset->eligible[i]) {
                    active = i;
                    break;
                }
        }
        if (!active)
            return EventResult::Ignored;
        const auto token = dataset->order[*active];
        if (event.key == Key::Enter) {
            if (runtime->mutable_available())
                runtime->source->activate(dataset->tokens, token,
                                          runtime->guard(contact->serial, runtime->epoch,
                                                         input_permission));
            return EventResult::Handled;
        }
        if (runtime->hierarchy.hierarchical &&
            (event.key == Key::Left || event.key == Key::Right)) {
            const auto row = dataset->visible[*active];
            if (event.key == Key::Right) {
                if (row.branch && !row.expanded)
                    runtime->toggle(token, event.alt, input_permission);
                else if (row.branch && *active + 1 < count &&
                         dataset->visible[*active + 1].depth > row.depth) {
                    contact->active = dataset->order[*active + 1];
                    if (runtime->writable())
                        runtime->choose(*contact->active, CollectionSelectionGesture::Replace,
                                    input_permission);
                    else
                        runtime->ensure_visible(*contact->active);
                }
            } else if (row.branch && row.expanded)
                runtime->toggle(token, event.alt, input_permission);
            else if (dataset->graph->parents[row.dataset_index]) {
                contact->active =
                    dataset->tokens->tokens[*dataset->graph->parents[row.dataset_index]];
                if (runtime->writable())
                    runtime->choose(*contact->active, CollectionSelectionGesture::Replace,
                                        input_permission);
                else
                    runtime->ensure_visible(*contact->active);
            }
            return EventResult::Handled;
        }
        if (event.primary && event.key == Key::A) {
            if (runtime->options.selection_mode == SelectionMode::Multiple)
                runtime->choose(token, CollectionSelectionGesture::SelectAll, input_permission);
            return EventResult::Handled;
        }
        std::size_t target = *active;
        int direction = 1;
        const auto columns = runtime->published.columns;
        if (event.key == Key::Home) {
            target = 0;
            direction = 1;
        } else if (event.key == Key::End) {
            target = count - 1;
            direction = -1;
        } else if (event.key == Key::Down || event.key == Key::Up) {
            direction = event.key == Key::Up ? -1 : 1;
            target = runtime->layout->neighbour(target, 0, direction, columns, count);
        } else if (event.key == Key::Left || event.key == Key::Right) {
            direction = event.key == Key::Left ? -1 : 1;
            target = runtime->layout->neighbour(target, direction, 0, columns, count);
        } else if (event.key == Key::PageDown || event.key == Key::PageUp) {
            direction = event.key == Key::PageUp ? -1 : 1;
            const double fitted =
                std::floor(std::max(0.0, static_cast<double>(runtime->viewport.h)) /
                           runtime->options.row_heights.estimate);
            const auto rows = fitted >= static_cast<double>(count)
                                  ? count
                                  : std::max<std::size_t>(1, static_cast<std::size_t>(fitted));
            const auto column_count = std::max<std::size_t>(1, columns);
            const auto distance = rows > count / column_count ? count : rows * column_count;
            target = direction < 0 ? target - std::min(target, distance)
                                   : target + std::min(count - 1 - target, distance);
        } else
            return EventResult::Ignored;
        while (target < count && !dataset->eligible[target]) {
            if (direction < 0) {
                if (!target)
                    break;
                --target;
            } else {
                if (target + 1 == count)
                    break;
                ++target;
            }
        }
        if (dataset->eligible[target]) {
            contact->active = dataset->order[target];
            const auto gesture = event.shift ? (event.primary ? CollectionSelectionGesture::AddRange
                                                              : CollectionSelectionGesture::Extend)
                                 : event.primary ? CollectionSelectionGesture::MoveActive
                                                 : CollectionSelectionGesture::Replace;
            if (runtime->writable())
                runtime->choose(dataset->order[target], gesture, input_permission);
            else
                runtime->ensure_visible(dataset->order[target]);
        }
        return EventResult::Handled;
    }
    void paint(PaintContext &context) const override {
        const auto runtime = runtime_;
        runtime->theme = &current_theme();
        const auto clip = context.painter().scoped_clip(context.bounds());
        VisualState state;
        state.enabled = effective_enabled();
        state.read_only = effective_read_only();
        state.focused = runtime->contact->focused;
        const auto defaults = default_list_view_style(current_theme());
        const auto surface = resolve_list_view_style(defaults, runtime->options.style, state);
        context.painter().fill_rounded_rect(context.bounds(), surface.surface_corner_radius,
                                            surface.surface_fill);
        if (!runtime->published.geometry || !runtime->published_dataset)
            return;
        const auto painted = runtime->published_dataset;
        for (const auto index : runtime->published.window) {
            if (index >= painted->order.size())
                continue;
            const auto token = painted->order[index];
            const auto rect = runtime->published.geometry->bounds_at(index);
            state.selected = std::binary_search(runtime->selected_tokens->begin(),
                                                runtime->selected_tokens->end(), token);
            state.hovered = runtime->contact->hovered == token;
            state.pressed = runtime->contact->pressed == token;
            state.enabled = effective_enabled() && painted->eligible[index];
            state.focused = runtime->contact->focused && runtime->contact->active == token;
            state.read_only = effective_read_only() || !runtime->source->valid() ||
                              !runtime->source->selection_valid();
            const auto row = resolve_list_view_style(defaults, runtime->options.style, state);
            context.painter().fill_rounded_rect(rect, row.row_corner_radius, row.row_fill);
            if (state.selected && row.row_accent_width > 0) {
                const double horizontal = nonnegative(row.row_accent_horizontal_inset),
                             vertical = nonnegative(row.row_accent_vertical_inset);
                context.painter().fill_rounded_rect(
                    {coordinate(rect.x + horizontal), coordinate(rect.y + vertical),
                     extent(std::min<double>(row.row_accent_width,
                                             std::max(0.0, rect.w - horizontal))),
                     extent(rect.h - 2 * vertical)},
                    0.0f, row.row_accent);
            }
            if (row.separator_width > 0) {
                const double inset = std::min<double>(rect.w / 2, nonnegative(row.separator_inset));
                context.painter().line(
                    {coordinate(rect.x + inset), coordinate(rect.y + rect.h)},
                    {coordinate(rect.x + rect.w - inset), coordinate(rect.y + rect.h)},
                    row.separator_width, row.separator);
            }
            if (painted->visible[index].branch) {
                const auto &visible = painted->visible[index];
                auto disclosure_rect = rect;
                if (runtime->columns_geometry &&
                    !runtime->columns_geometry->hierarchy_column().empty())
                    disclosure_rect = runtime->columns_geometry->published_column_bounds(
                        std::string(runtime->columns_geometry->hierarchy_column()), rect);
                const auto chevron_clip = context.painter().scoped_clip(disclosure_rect);
                const Point position{coordinate(disclosure_rect.x +
                                                visible.depth * runtime->hierarchy.indentation + 2),
                                     coordinate(rect.y + rect.h * 0.7)};
                context.painter().text(position, visible.expanded ? "v" : ">", 12.0f,
                                       current_theme().palette.text);
            }
        }
        if (surface.surface_border_width > 0)
            context.painter().stroke_rounded_rect(context.bounds(), surface.surface_corner_radius,
                                                  surface.surface_border_width,
                                                  surface.surface_border);
    }

  private:
    void measure_children_pass_started(const Constraints &constraints, std::size_t) const override {
        const auto runtime = runtime_;
        if (!runtime->options.row_heights.variable)
            return;
        const float width = constraints.bounded_width() ? constraints.max.w : runtime->viewport.w;
        const auto columns =
            runtime->columns_geometry ? runtime->columns_geometry->measurement_generation() : 0;
        if (runtime->measured_width && *runtime->measured_width == width &&
            runtime->measured_columns == columns)
            return;
        CollectionHeightIndex fresh;
        fresh.replace(std::vector<double>(runtime->dataset->order.size(),
                                          runtime->options.row_heights.estimate));
        runtime->heights = std::move(fresh);
        runtime->measured_width = width;
        runtime->measured_columns = columns;
    }
    void retained_checkpoint() override { runtime_->sync(); }
    void effective_availability_changed(const ComponentAvailability &,
                                        const ComponentAvailability &current) noexcept override {
        runtime_->availability = current;
        if (!current.interactive() || current.read_only)
            stop_contact_noexcept(runtime_->contact);
    }
    void layout_committed(Rect, Rect bounds) noexcept override {
        const auto runtime = runtime_;
        runtime->published = std::move(runtime->candidate);
        runtime->published_dataset = std::move(runtime->candidate_dataset);
        runtime->published_selected = std::move(runtime->candidate_selected);
        runtime->published_metadata = std::move(runtime->candidate_metadata);
        runtime->published_epoch = runtime->candidate_epoch;
        runtime->offset = runtime->candidate_offset;
        runtime->published_offset = runtime->candidate_offset;
        const bool changed = runtime->viewport.x != bounds.x || runtime->viewport.y != bounds.y ||
                             runtime->viewport.w != bounds.w || runtime->viewport.h != bounds.h;
        runtime->viewport = bounds;
        if (changed)
            runtime->pending |= 3;
    }
    std::shared_ptr<CollectionRuntime> runtime_;
};
class CollectionCellClip final : public Component {
  public:
    [[nodiscard]] bool clips_children() const noexcept override { return true; }
    [[nodiscard]] Size measure(const std::vector<ChildMetrics> &children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }
    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics> &children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }
    [[nodiscard]] Constraints child_constraints(const Constraints &constraints, std::size_t,
                                                std::size_t) const override {
        return constraints;
    }
    void layout_children(Rect bounds, const std::vector<ChildMetrics> &,
                         std::vector<ChildPlacement> &placements) const override {
        if (!placements.empty())
            placements.front().bounds = bounds;
    }
    void paint(PaintContext &) const override {}
};
class CollectionCellStrip final : public Component, public CollectionColumnsParticipant {
  public:
    explicit CollectionCellStrip(std::vector<std::string> columns) : columns_(std::move(columns)) {}
    void bind_collection_columns(std::shared_ptr<CollectionColumnsGeometry> layout) override {
        layout_ = std::move(layout);
    }
    void bind_collection_row_indent(double value) noexcept override { indent_ = value; }
    void bind_collection_row_padding(double horizontal, double vertical) noexcept override {
        horizontal_ = horizontal;
        vertical_ = vertical;
    }
    [[nodiscard]] Size measure(const std::vector<ChildMetrics> &children) const override {
        double width = 0;
        float height = 0;
        for (std::size_t i = 0; i < children.size(); ++i) {
            width += children[i].preferred.w;
            height = std::max(height, children[i].preferred.h);
            if (layout_ && i < columns_.size())
                layout_->record_natural_width(
                    columns_[i], children[i].preferred.w + 2 * horizontal_ +
                                     (layout_->hierarchy_column() == columns_[i] ? indent_ : 0.0));
        }
        return {extent(width), height};
    }
    [[nodiscard]] Constraints child_constraints(const Constraints &constraints, std::size_t index,
                                                std::size_t count) const override {
        const float width =
            layout_ && index < columns_.size()
                ? extent((count == 1 ? constraints.max.w : layout_->column_width(columns_[index])) -
                         2 * horizontal_ -
                         (layout_->hierarchy_column() == columns_[index] ? indent_ : 0.0))
                : constraints.max.w / static_cast<float>(std::max<std::size_t>(1, count));
        return {{0, 0}, {width, kUnboundedExtent}};
    }
    void layout_children(Rect bounds, const std::vector<ChildMetrics> &children,
                         std::vector<ChildPlacement> &placements) const override {
        const float width =
            bounds.w / static_cast<float>(std::max<std::size_t>(1, placements.size()));
        for (std::size_t i = 0; i < placements.size(); ++i) {
            if (layout_ && i < columns_.size()) {
                auto cell =
                    placements.size() == 1 ? bounds : layout_->column_bounds(columns_[i], bounds);
                const double indent = layout_->hierarchy_column() == columns_[i] ? indent_ : 0.0;
                const float available = extent(cell.w - 2 * horizontal_ - indent);
                const auto alignment = layout_->column_alignment(columns_[i]);
                const float child_width = alignment == Align::Stretch || i >= children.size()
                                              ? available
                                              : std::min(available, children[i].preferred.w);
                const float free = available - child_width;
                const float shift = alignment == Align::End      ? free
                                    : alignment == Align::Center ? free / 2
                                                                 : 0;
                placements[i].bounds = {coordinate(cell.x + horizontal_ + indent + shift),
                                        coordinate(cell.y + vertical_), child_width,
                                        extent(cell.h - 2 * vertical_)};
            } else
                placements[i].bounds = {coordinate(bounds.x + static_cast<double>(i) * width),
                                        bounds.y, width, bounds.h};
        }
    }
    void paint(PaintContext &) const override {}

  private:
    std::vector<std::string> columns_;
    std::shared_ptr<CollectionColumnsGeometry> layout_;
    double indent_{}, horizontal_{4}, vertical_{2};
};
} // namespace
std::size_t CollectionVerticalGeometry::size() const noexcept { return heights_.size(); }
Rect CollectionVerticalGeometry::bounds_at(std::size_t index) const noexcept {
    if (index >= heights_.size())
        return {};
    return {coordinate(static_cast<double>(viewport_.x) - offset_.x),
            coordinate(static_cast<double>(viewport_.y) + heights_.prefix_sum(index) - offset_.y),
            viewport_.w, extent(heights_.height_at(index))};
}
std::size_t CollectionLayoutPolicy::neighbour(std::size_t index, int horizontal, int vertical,
                                              std::size_t, std::size_t count) const noexcept {
    if (!count)
        return 0;
    const int step = vertical ? vertical : horizontal;
    return step < 0 ? index - std::min<std::size_t>(index, 1) : std::min(index + 1, count - 1);
}
void CollectionController::attach(std::shared_ptr<CollectionViewSession> session) {
    sessions_.erase(std::remove_if(sessions_.begin(), sessions_.end(),
                                   [](const auto &entry) { return entry.expired(); }),
                    sessions_.end());
    for (const auto &entry : sessions_)
        if (entry.lock() == session)
            return;
    sessions_.push_back(std::move(session));
}
void CollectionController::detach(const CollectionViewSession *session) noexcept {
    sessions_.erase(std::remove_if(sessions_.begin(), sessions_.end(),
                                   [session](const auto &entry) {
                                       const auto current = entry.lock();
                                       return !current || current.get() == session;
                                   }),
                    sessions_.end());
}
std::shared_ptr<CollectionViewSession> CollectionController::primary() const noexcept {
    for (const auto &entry : sessions_)
        if (const auto current = entry.lock(); current && current->attached())
            return current;
    return {};
}
Spec make_collection_cell_strip(std::vector<Spec> cells, std::vector<std::string> columns) {
    for (auto &cell : cells) {
        std::vector<Spec> child;
        child.push_back(std::move(cell));
        cell = Spec{[] { return std::make_unique<CollectionCellClip>(); }, std::move(child)};
    }
    return Spec{
        [columns = std::move(columns)] { return std::make_unique<CollectionCellStrip>(columns); },
        std::move(cells)};
}
Spec make_collection_view(CollectionSourceFactory source, CollectionViewOptions options,
                          CollectionHierarchyOptions hierarchy,
                          std::shared_ptr<CollectionLayoutPolicy> layout,
                          std::vector<std::string> columns) {
    Spec spec;
    spec.factory = [source = std::move(source), options = std::move(options), hierarchy,
                    layout = std::move(layout), columns = std::move(columns)] {
        auto runtime =
            std::make_shared<CollectionRuntime>(source(), options, hierarchy, layout, columns);
        return std::make_unique<CollectionViewComponent>(std::move(runtime));
    };
    spec.children_factory = [](Component &component) {
        return static_cast<CollectionViewComponent &>(component).initial_children();
    };
    return spec;
}
} // namespace ui::detail
