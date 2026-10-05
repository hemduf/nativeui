#pragma once

#include <nativeui/detail/dynamic_source.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/detail/virtual_list_row.hpp>
#include <nativeui/detail/virtual_list_window.hpp>
#include <nativeui/layout.hpp>
#include <nativeui/list_tabs_style.hpp>
#include <nativeui/state.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <limits>
#include <stdexcept>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ui::detail {

template <class Key>
class VirtualListRetainedRuntime;

template <class Key>
struct VirtualListPresentationState final {
    const Theme* theme{};
    ComponentAvailability availability{};
    bool focused{};
    std::optional<std::size_t> hovered_index;
    std::optional<Key> selected;
    std::function<void()> invalidate;
    bool mounted{};
};

template <class Key>
[[nodiscard]] ResolvedListViewStyle resolve_virtual_list_row_style(
    const VirtualListRetainedRuntime<Key>& runtime,
    const VirtualListPresentationState<Key>& presentation,
    std::size_t index,
    bool pressed);

template <class Key>
class VirtualListRetainedRuntime : public std::enable_shared_from_this<VirtualListRetainedRuntime<Key>> {
public:
    using Model = VirtualListDatasetModel<Key>;
    using Item = typename Model::Item;
    using RowFactory = std::function<Spec(const Item&)>;
    using ActivationCallback = std::function<void(const Key&)>;
    using MetadataSnapshot = VirtualSemanticChildren::MetadataSnapshot;

private:
    struct Dataset {
        Dataset(float height,RowFactory factory,std::size_t extra)
            : row_height(height),overscan(extra),row_factory(std::move(factory)) {}
        Model model;
        const float row_height;
        const std::size_t overscan;
        RowFactory row_factory;
        ScrollState scroll{ScrollAxis::Vertical};
        std::optional<Binding<std::optional<Key>>> selection;
        State<std::optional<Key>>* historical_selection{};
        std::vector<std::weak_ptr<VirtualListRetainedRuntime>> sessions;
        ScrollState::Subscription scroll_subscription;
        std::weak_ptr<VirtualListRetainedRuntime> metrics_authority;
        std::uint64_t next_attachment{1};
        [[nodiscard]] std::vector<std::shared_ptr<VirtualListRetainedRuntime>> live_sessions() {
            std::erase_if(sessions,[](const auto& session) { return session.expired(); });
            std::vector<std::shared_ptr<VirtualListRetainedRuntime>> result;
            result.reserve(sessions.size());
            for (const auto& session:sessions) if (auto current=session.lock()) result.push_back(std::move(current));
            return result;
        }
        void refresh_sessions() {
            const auto retained=live_sessions();
            // A throwing first factory/observer cannot hide the committed model
            // from the unstarted suffix: each view polls its own dirty journal.
            for (const auto& session:retained) { session->window_dirty_=true; ++session->window_serial_; }
            for (const auto& session:retained) if (session->attached_) { session->sync_shared_offset(); session->refresh_window(); }
        }
    };
    [[nodiscard]] static std::shared_ptr<Dataset> make_dataset(float row_height,RowFactory factory,std::size_t overscan) {
        auto dataset=std::make_shared<Dataset>(row_height,std::move(factory),overscan);
        const std::weak_ptr<Dataset> weak=dataset;
        dataset->scroll_subscription=dataset->scroll.observe([weak](Point) {
            if (const auto current=weak.lock()) current->refresh_sessions();
        });
        return dataset;
    }
    VirtualListRetainedRuntime(std::shared_ptr<Dataset> data,ListViewStyle style,ActivationCallback activation)
        : data_(std::move(data)),window_(data_->model,data_->row_height,data_->row_factory),
          style_(std::move(style)),activation_callback_(std::move(activation)),session_(true) {}
    [[nodiscard]] static std::shared_ptr<VirtualListRetainedRuntime> attached_primary(const std::shared_ptr<Dataset>& data) noexcept {
        std::shared_ptr<VirtualListRetainedRuntime> result;
        for (const auto& weak:data->sessions) if (auto current=weak.lock(); current && current->attached_ &&
            (!result || current->attachment_id_<result->attachment_id_)) result=std::move(current);
        return result;
    }
    [[nodiscard]] std::shared_ptr<VirtualListRetainedRuntime> primary_session() const noexcept {
        if (session_) return {};
        if (auto result=attached_primary(data_)) return result;
        for (const auto& weak:data_->sessions) if (auto current=weak.lock()) return current;
        return {};
    }
public:
    VirtualListRetainedRuntime(float row_height,RowFactory row_factory,std::size_t overscan=2)
        : data_(make_dataset(row_height,std::move(row_factory),overscan)),
          window_(data_->model,data_->row_height,data_->row_factory) {}
    VirtualListRetainedRuntime(const VirtualListRetainedRuntime&)=delete;
    VirtualListRetainedRuntime& operator=(const VirtualListRetainedRuntime&)=delete;

    // The controller's explicit data/selection/scroll authority is shared.
    // This session owns all presentation and materialization state for one UI.
    [[nodiscard]] std::shared_ptr<VirtualListRetainedRuntime> make_session(ListViewStyle style,ActivationCallback activation) const {
        auto result=std::shared_ptr<VirtualListRetainedRuntime>(new VirtualListRetainedRuntime(data_,std::move(style),std::move(activation)));
        data_->sessions.push_back(result);
        const std::weak_ptr<VirtualListRetainedRuntime> weak=result;
        result->local_scroll_subscription_=result->view_scroll_.observe([weak](Point) {
            if (const auto current=weak.lock()) current->local_offset_changed();
        });
        return result;
    }
    void bind_selection(State<std::optional<Key>>& source) noexcept {
        data_->historical_selection=&source; data_->selection=source.binding();
    }
    [[nodiscard]] State<std::optional<Key>>* selection_state() const noexcept { return data_->historical_selection; }
    [[nodiscard]] std::optional<Binding<std::optional<Key>>> selection_binding() const { return data_->selection; }
    [[nodiscard]] bool selection_valid() const noexcept { return data_->selection && data_->selection->valid(); }
    [[nodiscard]] bool interaction_valid() const noexcept { return selection_valid() && (!session_ || attached_); }
    void begin_keyboard_gesture() { ++interaction_serial_; }
    [[nodiscard]] std::optional<Key> selection_value() const { return data_->selection ? data_->selection->get() : std::optional<Key>{}; }
    void set_activation_callback(ActivationCallback callback) { activation_callback_=std::move(callback); }
    [[nodiscard]] const ActivationCallback& activation_callback() const noexcept { return activation_callback_; }
    void set_style(ListViewStyle style) { style_=std::move(style); }
    [[nodiscard]] const ListViewStyle& style() const noexcept { return style_; }

    [[nodiscard]] bool replace(std::vector<Item> items) {
        const auto data=data_;
        if (!virtual_list_content_height(items.size(),data->row_height)) return false;
        const auto retained=data->live_sessions();
        struct Anchors { std::optional<Key> focused,captured; };
        std::vector<Anchors> anchors; anchors.reserve(retained.size());
        for (const auto& current:retained) {
            Anchors candidate;
            if (current->focused_index_) if (const auto* item=data->model.item_at(*current->focused_index_)) candidate.focused=item->key;
            if (current->captured_index_) if (const auto* item=data->model.item_at(*current->captured_index_)) candidate.captured=item->key;
            anchors.push_back(std::move(candidate));
        }
        const auto generation=data->model.generation();
        if (!data->model.replace(std::move(items))) return false;
        if (data->model.generation()==generation) return true;
        for (std::size_t index=0;index<retained.size();++index) {
            const auto& current=retained[index];
            current->focused_index_=anchors[index].focused ? data->model.index_of_key(*anchors[index].focused) : std::nullopt;
            current->captured_index_=anchors[index].captured ? data->model.index_of_key(*anchors[index].captured) : std::nullopt;
            current->window_dirty_=true; ++current->window_serial_;
        }
        for (const auto& current:retained) if (current->attached_) { current->sync_shared_offset(); current->refresh_window(); }
        if (retained.empty()) { window_dirty_=true; ++window_serial_; refresh_window(); }
        return true;
    }
    [[nodiscard]] ScrollState& scroll() noexcept { return data_->scroll; }
    [[nodiscard]] const ScrollState& scroll() const noexcept { return data_->scroll; }
    [[nodiscard]] ScrollState& presentation_scroll() noexcept { return view_scroll_; }
    [[nodiscard]] const ScrollState& presentation_scroll() const noexcept { return view_scroll_; }
    void begin_viewport(Size viewport,Size content) {
        local_layout_pending_=true; candidate_viewport_=viewport; candidate_content_=content;
        const auto height=std::isfinite(viewport.h)?std::max(0.0f,viewport.h):0.0f;
        if (height!=viewport_height_) { viewport_height_=height; window_dirty_=true; ++window_serial_; }
        const auto lifetime=view_scroll_.lifetime_token();
        (void)ScrollMetricsAccess::publish(&view_scroll_,lifetime,viewport,content);
        update_metrics_authority(true);
        sync_shared_offset(); refresh_window();
    }
    void viewport_committed() noexcept {
        published_viewport_=candidate_viewport_; published_content_=view_scroll_.content_size();
        published_metrics_valid_=true; local_layout_pending_=false;
    }
    [[nodiscard]] float row_height() const noexcept { return data_->row_height; }
    [[nodiscard]] std::size_t overscan() const noexcept { return data_->overscan; }
    [[nodiscard]] std::size_t size() const noexcept { return data_->model.size(); }
    [[nodiscard]] std::uint64_t dataset_generation() const noexcept { return data_->model.generation(); }
    [[nodiscard]] const MetadataSnapshot& metadata_snapshot() const noexcept { return data_->model.metadata_snapshot(); }
    [[nodiscard]] VirtualSemanticChildren semantic_children(Rect bounds) const {
        return data_->model.semantic_children(selection_value(),bounds,row_height(),session_?view_scroll_.offset().y:scroll().offset().y);
    }
    [[nodiscard]] bool enabled_at(std::size_t index) const noexcept {
        const auto* item=data_->model.item_at(index); return item && item->enabled;
    }
    [[nodiscard]] std::optional<std::size_t> selected_index() const {
        const auto selected=selection_value(); if (!selected) return {};
        const auto index=data_->model.index_of_key(*selected); return index && enabled_at(*index) ? index : std::nullopt;
    }
    [[nodiscard]] std::optional<std::size_t> materialized_selected_index() const {
        const auto selected=selection_value(); return selected ? materialized_index_for_key(*selected) : std::nullopt;
    }
    [[nodiscard]] std::optional<std::size_t> first_enabled() const noexcept {
        for (std::size_t index=0;index<size();++index) if (enabled_at(index)) return index;
        return {};
    }
    [[nodiscard]] std::optional<std::size_t> last_enabled() const noexcept {
        for (std::size_t index=size();index>0;--index) if (enabled_at(index-1)) return index-1;
        return {};
    }
    [[nodiscard]] std::optional<std::size_t> next_enabled(std::size_t from) const noexcept {
        for (std::size_t index=from+1;index<size();++index) if (enabled_at(index)) return index;
        return {};
    }
    [[nodiscard]] std::optional<std::size_t> previous_enabled(std::size_t from) const noexcept {
        for (std::size_t index=from;index>0;--index) if (enabled_at(index-1)) return index-1;
        return {};
    }
    [[nodiscard]] bool scroll_to_index(std::size_t index,ScrollAlignment alignment=ScrollAlignment::Nearest) {
        update_metrics_authority(false);
        const auto target=data_->model.scroll_offset_for_index(index,row_height(),scroll().viewport_size().h,
            scroll().offset().y,virtual_alignment(alignment));
        if (!target) return false;
        const auto before=scroll().offset(); scroll().set_offset({before.x,*target}); return true;
    }
    [[nodiscard]] bool scroll_to_key(const Key& key,ScrollAlignment alignment=ScrollAlignment::Nearest) {
        const auto index=data_->model.index_of_key(key); return index && scroll_to_index(*index,alignment);
    }
    [[nodiscard]] bool select(std::size_t index) {
        const auto data=data_; auto source=data->selection;
        if (!source || !source->valid() || !enabled_at(index)) return false;
        const auto* item=data->model.item_at(index); if (!item) return false;
        const Key key=item->key; const auto generation=data->model.generation(),serial=interaction_serial_;
        (void)scroll_to_index(index,ScrollAlignment::Nearest);
        if (!interaction_valid() || generation!=data->model.generation() || serial!=interaction_serial_) return false;
        source->set(std::optional<Key>{key});
        return interaction_valid() && generation==data->model.generation() && serial==interaction_serial_;
    }
    [[nodiscard]] bool activate(std::size_t index) {
        const auto data=data_; if (!selection_valid() || !enabled_at(index)) return false;
        const auto* item=data->model.item_at(index); if (!item) return false;
        const Key key=item->key; const auto generation=data->model.generation(),serial=interaction_serial_;
        const auto callback=activation_callback_;
        if (!interaction_valid() || generation!=data->model.generation() || serial!=interaction_serial_ || !select(index)) return false;
        const auto authoritative=selection_value();
        if (authoritative && *authoritative==key && interaction_valid() && generation==data->model.generation() &&
            serial==interaction_serial_ && callback) callback(key);
        return true;
    }
    void reveal_selection() {
        const auto selected=selection_value(); if (selected) (void)scroll_to_key(*selected,ScrollAlignment::Nearest);
    }
    void set_focused_index(std::optional<std::size_t> index) {
        if (const auto primary=primary_session()) { primary->set_focused_index(index); return; }
        if (index && *index>=size()) index.reset();
        if (focused_index_==index) return; focused_index_=index; window_dirty_=true; ++window_serial_; refresh_window();
    }
    [[nodiscard]] std::optional<std::size_t> focused_index() const noexcept {
        if (const auto primary=primary_session()) return primary->focused_index(); return focused_index_;
    }
    void set_captured_index(std::optional<std::size_t> index) {
        if (const auto primary=primary_session()) { primary->set_captured_index(index); return; }
        if (index && *index>=size()) index.reset();
        if (captured_index_==index) return; captured_index_=index; window_dirty_=true; ++window_serial_; refresh_window();
    }
    [[nodiscard]] std::optional<std::size_t> captured_index() const noexcept {
        if (const auto primary=primary_session()) return primary->captured_index(); return captured_index_;
    }
    [[nodiscard]] std::optional<std::size_t> materialized_index_for_key(const Key& key) const {
        if (const auto primary=primary_session()) return primary->materialized_index_for_key(key);
        for (const auto& materialized:window_.items()) {
            const auto* item=data_->model.item_at(materialized.index);
            if (item && item->key==key) return materialized.index;
        }
        return {};
    }
    [[nodiscard]] bool begin_pointer_capture(const Key& key) {
        const auto index=materialized_index_for_key(key); if (!index || !enabled_at(*index)) return false;
        const auto serial=++interaction_serial_;
        set_captured_index(index); return interaction_valid() && serial==interaction_serial_;
    }
    void end_pointer_capture(const Key& key) {
        if (!captured_index_) return;
        const auto* item=data_->model.item_at(*captured_index_);
        if (!item || item->key==key) set_captured_index(std::nullopt);
    }
    [[nodiscard]] bool activate_key(const Key& key) {
        const auto index=materialized_index_for_key(key); return index && activate(*index);
    }
    [[nodiscard]] float content_height() const noexcept {
        return virtual_list_content_height(size(),row_height()).value_or(0.0f);
    }
    void set_viewport_height(float height) {
        if (const auto primary=primary_session()) { primary->set_viewport_height(height); return; }
        height=std::isfinite(height)?std::max(0.0f,height):0.0f;
        if (height==viewport_height_) return; viewport_height_=height; window_dirty_=true; ++window_serial_; refresh_window();
    }
    void set_presentation_invalidator(std::function<void()> invalidator) { presentation_invalidator_=std::move(invalidator); }
    void set_structure_invalidator(std::function<void()> invalidator) {
        const bool attach=static_cast<bool>(invalidator);
        if (attach && !attached_) {
            if (data_->next_attachment==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("Virtual list attachment identity exhausted");
            attachment_id_=data_->next_attachment++;
        }
        structure_invalidator_=std::move(invalidator); attached_=attach;
        if (attached_) { window_dirty_=true; ++window_serial_; refresh_window(); }
    }
    [[nodiscard]] std::vector<std::string> desired_keys() const {
        if (const auto primary=primary_session()) return primary->desired_keys(); return window_.keys();
    }
    [[nodiscard]] std::vector<DynamicChildSpec> desired_children(const std::shared_ptr<VirtualListRetainedRuntime>& self,
        const std::shared_ptr<VirtualListPresentationState<Key>>& presentation) const {
        if (const auto primary=primary_session()) return primary->desired_children(primary,presentation);
        std::vector<DynamicChildSpec> result; result.reserve(window_.items().size());
        for (const auto& materialized:window_.items()) {
            const auto* item=data_->model.item_at(materialized.index); if (!item) continue;
            const Key key=item->key;
            Spec barrier{[] { return std::make_unique<VirtualListRowContentBarrierComponent>(); },{*materialized.payload}};
            Spec row{[self,presentation,key] { return std::make_unique<VirtualListRowInteractionComponent>(
                [self,key] { return self->begin_pointer_capture(key); },[self,key] { self->end_pointer_capture(key); },
                [self,key] { return self->activate_key(key); },[self,presentation,key](bool before_pressed,bool after_pressed) {
                    const auto index=self->materialized_index_for_key(key); if (!index) return false;
                    return resolve_virtual_list_row_style(*self,*presentation,*index,before_pressed)!=
                           resolve_virtual_list_row_style(*self,*presentation,*index,after_pressed);
                },[self,presentation] { return presentation->mounted && presentation->availability.interactive() && self->interaction_valid(); }); },{std::move(barrier)}};
            result.push_back({materialized.key,std::move(row)});
        }
        return result;
    }
    [[nodiscard]] const typename VirtualListMaterializationWindow<Key,Spec>::MaterializedItem* materialized_at(std::size_t index) const noexcept {
        if (const auto primary=primary_session()) return primary->materialized_at(index);
        const auto& items=window_.items(); return index<items.size()?&items[index]:nullptr;
    }
    [[nodiscard]] std::size_t materialized_count() const noexcept {
        if (const auto primary=primary_session()) return primary->materialized_count(); return window_.items().size();
    }
    void refresh() {
        local_layout_pending_=false;
        update_metrics_authority(false); sync_shared_offset(); refresh_window();
    }
private:
    [[nodiscard]] static VirtualListAlignment virtual_alignment(ScrollAlignment alignment) noexcept {
        switch (alignment) {
        case ScrollAlignment::Nearest:return VirtualListAlignment::Nearest;
        case ScrollAlignment::Start:return VirtualListAlignment::Start;
        case ScrollAlignment::Center:return VirtualListAlignment::Center;
        case ScrollAlignment::End:return VirtualListAlignment::End;
        }
        return VirtualListAlignment::Nearest;
    }
    void sync_shared_offset() {
        if (!session_ || !attached_) return;
        const auto before=std::exchange(syncing_shared_offset_,true);
        try { view_scroll_.set_offset(data_->scroll.offset()); }
        catch (...) { syncing_shared_offset_=before; throw; }
        syncing_shared_offset_=before;
    }
    void local_offset_changed() {
        window_dirty_=true; ++window_serial_;
        if (!syncing_shared_offset_ && !local_layout_pending_ && attached_) {
            const auto offset=view_scroll_.offset();
            data_->scroll.set_offset(offset);
        }
        refresh_window();
    }
    void update_metrics_authority(bool candidate) {
        const auto data=data_;
        const auto primary=attached_primary(data);
        if (!primary) { data->metrics_authority.reset(); return; }
        if (candidate && primary.get()!=this) return;
        if (!candidate && !primary->published_metrics_valid_) return;
        const auto viewport=candidate?primary->candidate_viewport_:primary->published_viewport_;
        const auto content=candidate?primary->candidate_content_:primary->published_content_;
        data->metrics_authority=primary;
        (void)ScrollMetricsAccess::publish(&data->scroll,data->scroll.lifetime_token(),viewport,content);
    }
    void refresh_window() {
        if (const auto primary=primary_session()) { primary->refresh_window(); return; }
        if (!window_dirty_ || refreshing_window_ || (session_ && !attached_)) return;
        const auto retained=this->weak_from_this().lock();
        const auto data=data_; const auto previous_keys=window_.keys();
        const auto offset=session_?view_scroll_.offset().y:data->scroll.offset().y;
        const auto generation=data->model.generation(),serial=window_serial_;
        const auto range=virtual_list_materialization_range(size(),row_height(),offset,viewport_height_,data->overscan);
        const auto indices=range?virtual_list_materialized_indices(size(),*range,focused_index_,captured_index_):std::nullopt;
        // A visible row gaining focus/capture does not change the materialized
        // window. Its owner resolves paint state independently, so an equal
        // style must not dirty layout merely because the logical contact moved.
        if (indices && window_geometry_valid_ && generation==window_generation_ &&
            offset==window_offset_ && viewport_height_==window_viewport_height_ && *indices==window_.indices()) {
            window_dirty_=false;
            return;
        }
        refreshing_window_=true;
        try {
            const auto invalidate=presentation_invalidator_;
            if (attached_ && invalidate) invalidate();
            if (session_ && !attached_) { refreshing_window_=false; return; }
            if (generation!=data->model.generation() || serial!=window_serial_) {
                refreshing_window_=false; window_dirty_=true;
                const auto notify=structure_invalidator_; if (notify) notify();
                return;
            }
            if (window_.update(offset,viewport_height_,data->overscan,focused_index_,captured_index_)) {
                window_generation_=generation; window_offset_=offset;
                window_viewport_height_=viewport_height_; window_geometry_valid_=true;
                window_dirty_=data->model.generation()!=generation || serial!=window_serial_;
            }
            refreshing_window_=false;
            if (window_.keys()!=previous_keys || window_dirty_) {
                const auto notify=structure_invalidator_;
                if (notify) notify();
            }
        } catch (...) { refreshing_window_=false; window_dirty_=true; throw; }
    }
    std::shared_ptr<Dataset> data_;
    VirtualListMaterializationWindow<Key,Spec> window_;
    ListViewStyle style_;
    ActivationCallback activation_callback_;
    std::optional<std::size_t> focused_index_;
    std::optional<std::size_t> captured_index_;
    std::function<void()> structure_invalidator_;
    std::function<void()> presentation_invalidator_;
    float viewport_height_{};
    bool session_{};
    bool attached_{};
    bool window_dirty_{true};
    bool refreshing_window_{};
    std::uint64_t interaction_serial_{};
    std::uint64_t window_serial_{};
    std::uint64_t attachment_id_{};
    std::uint64_t window_generation_{};
    float window_offset_{};
    float window_viewport_height_{};
    bool window_geometry_valid_{};
    ScrollState view_scroll_{ScrollAxis::Vertical};
    ScrollState::Subscription local_scroll_subscription_;
    Size candidate_viewport_{};
    Size candidate_content_{};
    Size published_viewport_{};
    Size published_content_{};
    bool published_metrics_valid_{};
    bool local_layout_pending_{};
    bool syncing_shared_offset_{};
};

template <class Key>
[[nodiscard]] ResolvedListViewStyle resolve_virtual_list_row_style(
    const VirtualListRetainedRuntime<Key>& runtime,
    const VirtualListPresentationState<Key>& presentation,
    std::size_t index,
    bool pressed) {
    static const Theme fallback = default_theme();
    const auto& theme = presentation.theme ? *presentation.theme : fallback;
    const auto selected_index = runtime.materialized_selected_index();
    const bool selected = selected_index && *selected_index == index;
    const bool hovered = !selected && presentation.hovered_index &&
        *presentation.hovered_index == index;
    const bool effective_pressed = !selected && pressed;
    return resolve_list_view_style(
        default_list_view_style(theme),
        runtime.style(),
        VisualState{
            .enabled = presentation.availability.enabled && runtime.enabled_at(index),
            .read_only = presentation.availability.read_only,
            .hovered = hovered,
            .pressed = effective_pressed,
            .focused = presentation.focused,
            .selected = selected,
        });
}

class VirtualListOwnedSpec final {
public:
    explicit VirtualListOwnedSpec(Spec spec) : spec_(std::move(spec)) {}

    [[nodiscard]] Spec spec() && { return std::move(spec_); }

private:
    Spec spec_;
};

template <class Key>
class VirtualListRetainedComponent final : public Component, public DynamicChildrenSource {
public:
    VirtualListRetainedComponent(
        std::shared_ptr<VirtualListRetainedRuntime<Key>> runtime,
        std::shared_ptr<VirtualListPresentationState<Key>> presentation)
        : runtime_(std::move(runtime)), presentation_(std::move(presentation)) {}

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        Size result{0.0f, runtime_->content_height()};
        for (const auto& child : children) result.w = std::max(result.w, child.preferred.w);
        return result;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        Size result{0.0f, runtime_->content_height()};
        for (const auto& child : children) result.w = std::max(result.w, child.minimum.w);
        return result;
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>&,
        std::vector<ChildPlacement>& placements) const override {
        // The outer view owns this session's viewport and ScrollView metrics.
        // The controller offset is shared, while materialization and clipping
        // remain local to this retained tree.
        const auto row_height = runtime_->row_height();
        for (std::size_t child_index = 0; child_index < placements.size(); ++child_index) {
            const auto* item = runtime_->materialized_at(child_index);
            if (!item) {
                placements[child_index].bounds = {};
                continue;
            }
            const double row_y = static_cast<double>(item->index) * static_cast<double>(row_height);
            placements[child_index].bounds = Rect{
                bounds.x,
                bounds.y + static_cast<float>(row_y),
                bounds.w,
                row_height};
        }
    }

    [[nodiscard]] std::vector<std::string> desired_keys() const override {
        return runtime_->desired_keys();
    }

    [[nodiscard]] std::vector<DynamicChildSpec> desired_children() const override {
        return runtime_->desired_children(runtime_, presentation_);
    }

    void set_structure_invalidator(std::function<void()> invalidator) override {
        runtime_->set_structure_invalidator(std::move(invalidator));
    }

    void unmount(LifecycleContext&) override {
        runtime_->set_structure_invalidator({});
    }

    void paint(PaintContext&) const override {}

private:
    std::shared_ptr<VirtualListRetainedRuntime<Key>> runtime_;
    std::shared_ptr<VirtualListPresentationState<Key>> presentation_;
};

template <class Key>
class VirtualListViewComponent final : public Component, public ThemeBinding {
public:
    VirtualListViewComponent(
        std::shared_ptr<VirtualListRetainedRuntime<Key>> runtime,
        std::shared_ptr<VirtualListPresentationState<Key>> presentation)
        : runtime_(std::move(runtime)), presentation_(std::move(presentation)) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] bool pointer_targetable() const noexcept override { return true; }

    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().preferred;
    }

    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override {
        return children.empty() ? Size{} : children.front().minimum;
    }

    void layout_children(
        Rect bounds,
        const std::vector<ChildMetrics>& children,
        std::vector<ChildPlacement>& placements) const override {
        const auto runtime=runtime_;
        if (!placements.empty()) placements.front().bounds = bounds;
        const auto width=children.empty()?bounds.w:std::max(bounds.w,children.front().preferred.w);
        runtime->begin_viewport({bounds.w,bounds.h},{width,runtime->content_height()});
    }

    [[nodiscard]] bool uses_retained_checkpoint() const noexcept override { return true; }

    [[nodiscard]] std::vector<Spec> children() const {
        const auto runtime=runtime_; const auto presentation=presentation_;
        const auto initial=runtime->desired_children(runtime,presentation);
        std::vector<Spec> rows; rows.reserve(initial.size());
        for (const auto& row:initial) rows.push_back(row.spec);
        Spec content{[runtime,presentation] { return std::make_unique<VirtualListRetainedComponent<Key>>(runtime,presentation); },std::move(rows)};
        return {std::move(ScrollView{runtime->presentation_scroll(),VirtualListOwnedSpec{std::move(content)}}).spec()};
    }

    void mount(MountContext& context) override {
        presentation_->theme = &current_theme();
        presentation_->availability = effective_availability();
        presentation_->focused = false;
        presentation_->mounted=true;
        presentation_->selected=runtime_->selection_value();
        presentation_->invalidate=context.invalidator();
        runtime_->set_presentation_invalidator(context.layout_invalidator());
        if (auto selection=runtime_->selection_binding()) {
            const std::weak_ptr<VirtualListRetainedRuntime<Key>> weak_runtime=runtime_;
            const std::weak_ptr<VirtualListPresentationState<Key>> weak_presentation=presentation_;
            selection_subscription_=selection->observe([weak_runtime,weak_presentation](const std::optional<Key>&) {
                const auto runtime=weak_runtime.lock(); const auto presentation=weak_presentation.lock();
                if (!runtime || !presentation || !presentation->mounted) return;
                const auto invalidate=presentation->invalidate;
                presentation->selected=runtime->selection_value();
                if (invalidate) invalidate();
                if (presentation->mounted) runtime->reveal_selection();
            });
        }
    }

    void unmount(LifecycleContext&) override {
        presentation_->mounted=false;
        presentation_->invalidate={};
        runtime_->set_presentation_invalidator({});
        selection_subscription_.reset();
        presentation_->theme = nullptr;
        presentation_->hovered_index.reset();
        presentation_->focused = false;
    }

    void focus_changed(bool focused, FocusContext&) override {
        presentation_->focused = focused;
        if (!focused) {
            runtime_->set_focused_index(std::nullopt);
            return;
        }
        auto active = runtime_->selected_index();
        if (!active) active = runtime_->first_enabled();
        runtime_->set_focused_index(active);
    }

    EventResult input(const InputEvent& event, InputContext& context) override {
        auto runtime = runtime_;
        switch (event.type) {
        case InputType::PointerMove: {
            const auto index = row_at(event.position, context.bounds());
            set_hovered(index && runtime->enabled_at(*index) ? index : std::nullopt, context);
            return EventResult::Ignored;
        }
        case InputType::PointerLeave:
            set_hovered(std::nullopt, context);
            return EventResult::Ignored;
        default:
            break;
        }

        if (event.type != InputType::KeyDown || !runtime->selection_valid()) return EventResult::Ignored;

        auto active = runtime->selected_index();
        if (active) {
            runtime->set_focused_index(active);
        } else {
            active = runtime->focused_index();
            if (!active) {
                active = runtime->first_enabled();
                runtime->set_focused_index(active);
            }
        }

        if (event.key == ui::Key::Enter || event.key == ui::Key::Space) {
            runtime->begin_keyboard_gesture();
            if (!active) return EventResult::Ignored;
            return runtime->activate(*active)
                ? EventResult::Handled
                : EventResult::Ignored;
        }

        std::optional<std::size_t> target;
        switch (event.key) {
        case ui::Key::Down:
            target = active
                ? runtime->next_enabled(*active)
                : runtime->first_enabled();
            break;
        case ui::Key::Up:
            target = active
                ? runtime->previous_enabled(*active)
                : runtime->last_enabled();
            break;
        case ui::Key::Home:
            target = runtime->first_enabled();
            break;
        case ui::Key::End:
            target = runtime->last_enabled();
            break;
        default:
            return EventResult::Ignored;
        }

        if (target) {
            runtime->begin_keyboard_gesture();
            runtime->set_focused_index(target);
            (void)runtime->select(*target);
        }
        return EventResult::Handled;
    }

    void deactivate(LifecycleContext&) override {
        presentation_->hovered_index.reset();
        presentation_->focused = false;
    }

    void paint(PaintContext& context) const override {
        presentation_->theme = &current_theme();
        presentation_->availability = effective_availability();
        presentation_->focused = context.focused();

        const auto bounds = context.bounds();
        const auto surface = resolved_style(
            false,
            false,
            false,
            true,
            context.focused());
        auto& painter = context.painter();
        painter.fill_rounded_rect(bounds, surface.surface_corner_radius, surface.surface_fill);

        for (std::size_t child_index = 0; child_index < runtime_->materialized_count(); ++child_index) {
            const auto* materialized = runtime_->materialized_at(child_index);
            if (!materialized) continue;
            const auto index = materialized->index;
            const bool selected = runtime_->materialized_selected_index() &&
                *runtime_->materialized_selected_index() == index;
            const bool pressed = !selected && runtime_->captured_index() &&
                *runtime_->captured_index() == index;
            const auto row = resolve_virtual_list_row_style(
                *runtime_, *presentation_, index, pressed);
            paint_row(painter, bounds, index, row, selected);
        }

        painter.stroke_rounded_rect(
            bounds,
            surface.surface_corner_radius,
            surface.surface_border_width,
            surface.surface_border);
    }

private:
    void layout_committed(Rect,Rect) noexcept override { runtime_->viewport_committed(); }
    void retained_checkpoint() override {
        const auto runtime=runtime_; const auto presentation=presentation_;
        runtime->refresh();
        const auto selected=runtime->selection_value();
        if (selected==presentation->selected) return;
        const auto invalidate=presentation->invalidate;
        presentation->selected=selected;
        if (presentation->mounted && invalidate) invalidate();
        if (presentation->mounted) runtime->reveal_selection();
    }

    [[nodiscard]] ResolvedListViewStyle resolved_style(
        bool selected,
        bool hovered,
        bool pressed,
        bool row_enabled,
        bool focused) const {
        return resolve_list_view_style(
            default_list_view_style(current_theme()),
            runtime_->style(),
            VisualState{
                .enabled = effective_enabled() && row_enabled,
                .read_only = effective_read_only(),
                .hovered = hovered,
                .pressed = pressed,
                .focused = focused,
                .selected = selected,
            });
    }

    [[nodiscard]] std::optional<ResolvedListViewStyle> row_style(
        std::optional<std::size_t> index) const {
        if (!index || *index >= runtime_->size()) return std::nullopt;
        const bool pressed = runtime_->captured_index() &&
            *runtime_->captured_index() == *index;
        return resolve_virtual_list_row_style(
            *runtime_, *presentation_, *index, pressed);
    }

    void set_hovered(std::optional<std::size_t> index, InputContext& context) {
        if (presentation_->hovered_index == index) return;
        const auto previous = presentation_->hovered_index;
        const auto previous_before = row_style(previous);
        const auto next_before = index != previous ? row_style(index) : std::nullopt;
        presentation_->hovered_index = index;
        const auto previous_after = row_style(previous);
        const auto next_after = index != previous ? row_style(index) : std::nullopt;
        if (previous_before != previous_after || next_before != next_after) {
            context.invalidate();
        }
    }

    [[nodiscard]] std::optional<std::size_t> row_at(Point position, Rect bounds) const noexcept {
        if (!bounds.contains(position)) return std::nullopt;
        const double content_y = static_cast<double>(position.y - bounds.y) +
                                 static_cast<double>(runtime_->presentation_scroll().offset().y);
        const double row_height = static_cast<double>(runtime_->row_height());
        if (!std::isfinite(content_y) || content_y < 0.0 || !(row_height > 0.0)) {
            return std::nullopt;
        }
        const double index_value = std::floor(content_y / row_height);
        if (index_value < 0.0 || index_value >= static_cast<double>(runtime_->size())) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(index_value);
    }

    void paint_row(
        Painter& painter,
        Rect bounds,
        std::size_t index,
        const ResolvedListViewStyle& style,
        bool selected) const {
        const double local_y = static_cast<double>(index) *
                                   static_cast<double>(runtime_->row_height()) -
                               static_cast<double>(runtime_->presentation_scroll().offset().y);
        if (!std::isfinite(local_y)) return;

        const Rect highlight{
            bounds.x + style.row_horizontal_inset,
            bounds.y + static_cast<float>(local_y) + style.row_vertical_inset,
            std::max(0.0f, bounds.w - style.row_horizontal_inset * 2.0f),
            std::max(0.0f, runtime_->row_height() - style.row_vertical_inset * 2.0f)};
        const auto visible = intersect(highlight, bounds);
        if (visible.empty()) return;

        painter.fill_rounded_rect(visible, style.row_corner_radius, style.row_fill);
        if (selected && style.row_accent_width > 0.0f) {
            const Rect accent{
                visible.x + style.row_accent_horizontal_inset,
                visible.y + style.row_accent_vertical_inset,
                style.row_accent_width,
                std::max(0.0f, visible.h - style.row_accent_vertical_inset * 2.0f)};
            if (!accent.empty()) {
                painter.fill_rounded_rect(
                    accent,
                    std::max(0.0f, style.row_accent_width * 0.5f),
                    style.row_accent);
            }
        }

        if (index + 1 < runtime_->size() &&
            style.separator_width > 0.0f &&
            visible.w > style.separator_inset * 2.0f) {
            const float y = visible.y + visible.h - style.separator_width * 0.5f;
            painter.line(
                {visible.x + style.separator_inset, y},
                {visible.x + visible.w - style.separator_inset, y},
                style.separator_width,
                style.separator);
        }
    }

    [[nodiscard]] bool availability_change_affects_layout(
        const ComponentAvailability&,
        const ComponentAvailability& after) const override {
        presentation_->availability = after;
        return false;
    }

    std::shared_ptr<VirtualListRetainedRuntime<Key>> runtime_;
    std::shared_ptr<VirtualListPresentationState<Key>> presentation_;
    typename State<std::optional<Key>>::Subscription selection_subscription_;
};

template <class Key>
[[nodiscard]] Spec make_virtual_list_retained_spec(
    std::shared_ptr<VirtualListRetainedRuntime<Key>> runtime,ListViewStyle style,
    typename VirtualListRetainedRuntime<Key>::ActivationCallback activation) {
    Spec result{[runtime,style,activation] {
        auto session=runtime->make_session(style,activation);
        return std::make_unique<VirtualListViewComponent<Key>>(
            std::move(session),std::make_shared<VirtualListPresentationState<Key>>());
    },{}};
    result.children_factory=[](Component& component) {
        return static_cast<VirtualListViewComponent<Key>&>(component).children();
    };
    return result;
}
template <class Key>
[[nodiscard]] Spec make_virtual_list_retained_spec(std::shared_ptr<VirtualListRetainedRuntime<Key>> runtime) {
    const auto style=runtime->style(); const auto activation=runtime->activation_callback();
    return make_virtual_list_retained_spec(std::move(runtime),style,activation);
}

} // namespace ui::detail
