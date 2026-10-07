#pragma once

#include <nativeui/collection_model.hpp>
#include <nativeui/component.hpp>
#include <nativeui/list_tabs_style.hpp>
#include <nativeui/scroll_view.hpp>
#include <nativeui/detail/layout_types.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ui::detail {
class CollectionColumnsGeometry {
  public:
    virtual ~CollectionColumnsGeometry() = default;
    [[nodiscard]] virtual Rect column_bounds(const std::string &, Rect row) const noexcept = 0;
    // Input and painting consume only the geometry from a successful layout.
    [[nodiscard]] virtual Rect published_column_bounds(const std::string &id,
                                                       Rect row) const noexcept {
        return column_bounds(id, row);
    }
    [[nodiscard]] virtual double column_width(const std::string &) const noexcept = 0;
    virtual void record_natural_width(const std::string &, double) noexcept = 0;
    [[nodiscard]] virtual std::string_view hierarchy_column() const noexcept { return {}; }
    [[nodiscard]] virtual double total_width() const noexcept { return 0; }
    [[nodiscard]] virtual double horizontal_offset() const noexcept { return 0; }
    [[nodiscard]] virtual double published_horizontal_offset() const noexcept {
        return horizontal_offset();
    }
    virtual bool set_horizontal_offset(double) noexcept { return false; }
    [[nodiscard]] virtual Align column_alignment(const std::string &) const noexcept {
        return Align::Start;
    }
    [[nodiscard]] virtual std::uint64_t measurement_generation() const noexcept { return 0; }
};
class CollectionColumnsParticipant {
  public:
    virtual ~CollectionColumnsParticipant() = default;
    virtual void bind_collection_columns(std::shared_ptr<CollectionColumnsGeometry>) = 0;
    virtual void bind_collection_row_indent(double) noexcept {}
    virtual void bind_collection_row_padding(double, double) noexcept {}
};
struct CollectionSourceVersion {
    std::uint64_t rows{};
    std::uint64_t expansion{};
    bool operator==(const CollectionSourceVersion &) const = default;
};
struct CollectionInput {
    CollectionSourceVersion version;
    std::shared_ptr<const CollectionKeys> keys;
    std::vector<CollectionSearchItem> rows;
    std::vector<bool> sections, branches;
    std::function<std::vector<std::optional<std::size_t>>(
        std::shared_ptr<const CollectionTokenSnapshot>)>
        parents;
    std::function<std::vector<CollectionToken>(std::shared_ptr<const CollectionTokenSnapshot>)>
        expanded;
    std::function<Spec(std::size_t, const std::vector<std::string> &)> make_row;
};
class CollectionSubscription {
  public:
    virtual ~CollectionSubscription() = default;
};
struct CollectionExpansionRead {
    CollectionSourceVersion version;
    std::vector<CollectionToken> expanded;
};
struct CollectionSelectionRead {
    std::uint64_t revision{};
    CollectionSelection selection;
};
class CollectionSource {
  public:
    virtual ~CollectionSource() = default;
    [[nodiscard]] virtual CollectionSourceVersion version() const noexcept = 0;
    [[nodiscard]] virtual std::uint64_t selection_revision() const noexcept = 0;
    [[nodiscard]] virtual bool valid() const noexcept = 0;
    [[nodiscard]] virtual bool expansion_valid() const noexcept = 0;
    [[nodiscard]] virtual bool selection_valid() const noexcept = 0;
    [[nodiscard]] virtual std::optional<CollectionInput> read() const = 0;
    [[nodiscard]] virtual std::optional<CollectionExpansionRead>
        read_expansion(std::shared_ptr<const CollectionTokenSnapshot>) const = 0;
    [[nodiscard]] virtual std::optional<CollectionSelectionRead>
        read_selection(std::shared_ptr<const CollectionTokenSnapshot>) const = 0;
    [[nodiscard]] virtual bool publish_selection(std::shared_ptr<const CollectionTokenSnapshot>,
                                                 CollectionSelection,
                                                 const std::function<bool()> &) = 0;
    [[nodiscard]] virtual bool publish_expanded(std::shared_ptr<const CollectionTokenSnapshot>,
                                                std::vector<CollectionToken>,
                                                const std::function<bool()> &) = 0;
    virtual void activate(std::shared_ptr<const CollectionTokenSnapshot>, CollectionToken,
                          const std::function<bool()> &) = 0;
    virtual void reorder(std::shared_ptr<const CollectionTokenSnapshot>,
                         std::vector<CollectionToken>, std::optional<CollectionToken>,
                         const std::function<bool()> &) = 0;
    [[nodiscard]] virtual std::unique_ptr<CollectionSubscription>
        observe(std::function<void()>) = 0;
};
using CollectionSourceFactory = std::function<std::shared_ptr<CollectionSource>()>;
class CollectionViewSession {
  public:
    virtual ~CollectionViewSession() = default;
    [[nodiscard]] virtual bool attached() const noexcept = 0;
    [[nodiscard]] virtual std::shared_ptr<const CollectionTokenSnapshot>
    token_snapshot() const noexcept = 0;
    [[nodiscard]] virtual std::vector<CollectionToken> visible_tokens() const = 0;
    [[nodiscard]] virtual std::optional<std::size_t>
        depth_for_token(CollectionToken) const noexcept = 0;
    [[nodiscard]] virtual CollectionRange visible_range() const noexcept = 0;
    [[nodiscard]] virtual VirtualSemanticChildren semantic_children() const = 0;
    virtual bool scroll_to(CollectionToken, ScrollAlignment) = 0;
};
class CollectionController {
  public:
    void attach(std::shared_ptr<CollectionViewSession>);
    void detach(const CollectionViewSession *) noexcept;
    [[nodiscard]] std::shared_ptr<CollectionViewSession> primary() const noexcept;

  private:
    std::vector<std::weak_ptr<CollectionViewSession>> sessions_;
};
struct CollectionViewOptions {
    SelectionMode selection_mode{SelectionMode::Single};
    ListRowHeights row_heights;
    ListViewStyle style;
    bool typeahead{true};
    bool follow_end{};
    bool reorder{};
    bool empty_content{};
    std::shared_ptr<CollectionController> controller;
};
// Immutable data only: no Binding, Key, factory, UI owner, or native resource.
class CollectionVerticalGeometry final : public VirtualSemanticGeometry {
  public:
    CollectionVerticalGeometry(CollectionHeightSnapshot heights, Rect viewport, Point offset)
        : heights_(std::move(heights)), viewport_(viewport), offset_(offset) {}
    [[nodiscard]] std::size_t size() const noexcept override;
    [[nodiscard]] Rect bounds_at(std::size_t index) const noexcept override;

  private:
    CollectionHeightSnapshot heights_;
    Rect viewport_;
    Point offset_;
};
} // namespace ui::detail

namespace ui::detail {
struct CollectionVisibleRow {
    std::size_t dataset_index{};
    std::size_t depth{};
    bool branch{};
    bool expanded{};
    bool operator==(const CollectionVisibleRow &) const = default;
};
struct CollectionHierarchyOptions {
    bool hierarchical{};
    bool virtualized{true};
    bool reject_sections{};
    double indentation{16.0};
    double chevron_width{16.0};
};
struct CollectionLayoutSnapshot {
    std::shared_ptr<const VirtualSemanticGeometry> geometry;
    Size content;
    std::vector<std::size_t> window;
    std::size_t columns{1};
};
// Per-component strategies own their real geometry and navigation. The common
// retained engine owns only snapshots, selection, contact and materialisation.
class CollectionLayoutPolicy {
  public:
    virtual ~CollectionLayoutPolicy() = default;
    [[nodiscard]] virtual CollectionLayoutSnapshot prepare(CollectionHeightSnapshot heights,
                                                           std::size_t count, Rect viewport,
                                                           Point offset,
                                                           bool virtualized) const = 0;
    [[nodiscard]] virtual std::size_t neighbour(std::size_t index, int horizontal, int vertical,
                                                std::size_t columns,
                                                std::size_t count) const noexcept;
};
[[nodiscard]] std::shared_ptr<CollectionLayoutPolicy> make_tree_view_layout();
[[nodiscard]] std::shared_ptr<CollectionLayoutPolicy> make_outline_view_layout();
[[nodiscard]] Spec make_collection_view(CollectionSourceFactory source,
                                        CollectionViewOptions options,
                                        CollectionHierarchyOptions hierarchy,
                                        std::shared_ptr<CollectionLayoutPolicy> layout,
                                        std::vector<std::string> columns = {});
[[nodiscard]] Spec make_tree_view(CollectionSourceFactory source, CollectionViewOptions options,
                                  double indentation, double chevron_width);
[[nodiscard]] Spec make_outline_view(CollectionSourceFactory source, CollectionViewOptions options,
                                     double indentation, double chevron_width);
[[nodiscard]] Spec make_grid_view(CollectionSourceFactory source, CollectionViewOptions options,
                                  double minimum_width, double cell_height, double gap,
                                  bool reorder);
} // namespace ui::detail
