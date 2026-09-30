#pragma once

#include <nativeui/detail/semantic_action_view_binding.hpp>
#include <nativeui/detail/semantic_native_query.hpp>
#include <nativeui/detail/semantic_uia_mapping.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ui::detail {

/// Stable semantic identity carried by one lazily materialized UIA provider.
///
/// Ordinary providers carry their own `SemanticId`. Virtual collection
/// providers carry the owning ListView `SemanticId` plus the immutable logical
/// token minted by T067. No native object, retained-tree pointer or component
/// pointer is ever stored here.
struct UiaProviderIdentity final {
    SemanticId node_id{kInvalidSemanticId};
    std::optional<VirtualSemanticItemToken> virtual_token;

    [[nodiscard]] bool valid() const noexcept {
        if (node_id == kInvalidSemanticId) {
            return false;
        }
        return !virtual_token ||
               *virtual_token != kInvalidVirtualSemanticItemToken;
    }

    [[nodiscard]] SemanticIdentity semantic_identity() const noexcept {
        return SemanticIdentity{node_id, virtual_token};
    }

    bool operator==(const UiaProviderIdentity&) const noexcept = default;
};

struct UiaProviderIdentityHash final {
    [[nodiscard]] std::size_t operator()(
        const UiaProviderIdentity& identity) const noexcept {
        std::size_t seed = std::hash<SemanticId>{}(identity.node_id);
        const auto mix = [&seed](std::size_t value) noexcept {
            seed ^= value + static_cast<std::size_t>(0x9e3779b9U) +
                    (seed << 6U) + (seed >> 2U);
        };
        mix(std::hash<bool>{}(identity.virtual_token.has_value()));
        if (identity.virtual_token) {
            mix(std::hash<VirtualSemanticItemToken>{}(*identity.virtual_token));
        }
        return seed;
    }
};

/// Platform-neutral lifetime marker for one materialized native provider.
///
/// The per-view endpoint owns every live handle through `shared_ptr`; the Win32
/// adapter's handle wraps a COM object and translates the shared ownership to a
/// COM reference. `on_retired()` runs exactly once when the endpoint drops its
/// last cache reference because the semantic identity disappeared (or the
/// owning view retired), which lets the adapter tell UIA that the element is
/// gone before the native object is released.
class UiaProviderHandle {
public:
    UiaProviderHandle() noexcept = default;
    UiaProviderHandle(const UiaProviderHandle&) = delete;
    UiaProviderHandle& operator=(const UiaProviderHandle&) = delete;
    virtual ~UiaProviderHandle() noexcept = default;

    /// Notify the platform handle that its semantic identity is no longer
    /// available. Implementations must not throw.
    virtual void on_retired() noexcept {}
};

using UiaProviderHandlePtr = std::shared_ptr<UiaProviderHandle>;

/// Lifetime-safe state carried by one UIA provider.
///
/// The state stores only weak per-view publication/action endpoints plus stable
/// semantic identity. Every read loads the exact current immutable native
/// publication once and resolves through `SemanticNativeSnapshotQuery`; it
/// never retains a bridge, ViewCore, Tree, Node, Component or native view.
/// Once the owning view retires either endpoint, future work fails closed while
/// an already-started callback may finish from the immutable generation or
/// endpoint lease it already retained.
class UiaProviderState final {
public:
    UiaProviderState(const UiaProviderState&) = default;
    UiaProviderState& operator=(const UiaProviderState&) = default;
    UiaProviderState(UiaProviderState&&) noexcept = default;
    UiaProviderState& operator=(UiaProviderState&&) noexcept = default;

    [[nodiscard]] static std::optional<UiaProviderState> ordinary(
        std::weak_ptr<const SemanticNativePublicationSource> publication_source,
        SemanticId node_id,
        std::weak_ptr<const SemanticActionViewEndpoint> action_endpoint = {}) noexcept {
        if (node_id == kInvalidSemanticId) {
            return std::nullopt;
        }
        return UiaProviderState{
            std::move(publication_source), node_id, std::nullopt,
            std::move(action_endpoint)};
    }

    [[nodiscard]] static std::optional<UiaProviderState> virtual_item(
        std::weak_ptr<const SemanticNativePublicationSource> publication_source,
        SemanticId list_node_id,
        VirtualSemanticItemToken token,
        std::weak_ptr<const SemanticActionViewEndpoint> action_endpoint = {}) noexcept {
        if (list_node_id == kInvalidSemanticId ||
            token == kInvalidVirtualSemanticItemToken) {
            return std::nullopt;
        }
        return UiaProviderState{
            std::move(publication_source), list_node_id, token,
            std::move(action_endpoint)};
    }

    [[nodiscard]] SemanticId node_id() const noexcept { return node_id_; }

    [[nodiscard]] std::optional<VirtualSemanticItemToken>
    virtual_token() const noexcept {
        return virtual_token_;
    }

    [[nodiscard]] UiaProviderIdentity identity() const noexcept {
        return UiaProviderIdentity{node_id_, virtual_token_};
    }

    [[nodiscard]] bool action_available() const noexcept {
        return !action_endpoint_.expired();
    }

    /// Resolve this identity against the latest immutable per-view publication.
    /// A retired view, a removed identity or a missing/inconsistent T067 token
    /// index reports absent rather than scanning or guessing.
    [[nodiscard]] std::optional<SemanticNativeSnapshotRead> read() const {
        try {
            const auto source = publication_source_.lock();
            if (!source) {
                return std::nullopt;
            }
            auto publication = source->current();
            if (!publication) {
                return std::nullopt;
            }
            if (virtual_token_) {
                return SemanticNativeSnapshotQuery::virtual_item(
                    std::move(publication), node_id_, *virtual_token_);
            }
            return SemanticNativeSnapshotQuery::ordinary(
                std::move(publication), node_id_);
        } catch (...) {
            return std::nullopt;
        }
    }

    /// Post one typed request through the owning view's T065 endpoint. The
    /// router revalidates the advertised action, the exact generation and the
    /// current live semantics on the UI thread; this boundary only fails
    /// closed and never lets an exception cross a foreign callback ABI.
    [[nodiscard]] bool post(SemanticActionRequest request) const noexcept {
        try {
            if (!identity().valid()) {
                return false;
            }
            const auto endpoint = action_endpoint_.lock();
            if (!endpoint) {
                return false;
            }
            return endpoint->post(identity().semantic_identity(),
                                  std::move(request));
        } catch (...) {
            return false;
        }
    }

    [[nodiscard]] bool post_action(
        SemanticAction action,
        std::optional<double> numeric_value = std::nullopt,
        std::optional<std::string> text_value = std::nullopt) const noexcept {
        SemanticActionRequest request;
        request.action = action;
        request.numeric_value = numeric_value;
        request.text_value = std::move(text_value);
        return post(std::move(request));
    }

private:
    UiaProviderState(
        std::weak_ptr<const SemanticNativePublicationSource> publication_source,
        SemanticId node_id,
        std::optional<VirtualSemanticItemToken> virtual_token,
        std::weak_ptr<const SemanticActionViewEndpoint> action_endpoint) noexcept
        : publication_source_(std::move(publication_source)),
          action_endpoint_(std::move(action_endpoint)),
          node_id_(node_id),
          virtual_token_(virtual_token) {}

    std::weak_ptr<const SemanticNativePublicationSource> publication_source_;
    std::weak_ptr<const SemanticActionViewEndpoint> action_endpoint_;
    SemanticId node_id_{kInvalidSemanticId};
    std::optional<VirtualSemanticItemToken> virtual_token_;
};

/// One callback-local UIA read projected from exactly one native publication
/// generation. Role mapping, pattern eligibility, semantic properties and the
/// logical geometry pair therefore cannot mix across generations even if the
/// UI publishes while UIA is servicing a query. The read retains only immutable
/// publication data and stays valid after later publication or view teardown.
class UiaProviderRead final {
public:
    [[nodiscard]] static std::optional<UiaProviderRead> from_state(
        const UiaProviderState& state) {
        try {
            auto read = state.read();
            if (!read) {
                return std::nullopt;
            }

            const auto& info = read->info();
            const auto role_mapping = semantic_uia_role_mapping(info.role);
            if (!role_mapping) {
                // SemanticRole::None is flattened and must never materialize a
                // native provider of its own.
                return std::nullopt;
            }
            const auto patterns = semantic_uia_pattern_eligibility(
                info, read->virtual_token().has_value());
            const bool fragment_root = read->is_semantic_root();
            return UiaProviderRead{std::move(*read), *role_mapping, patterns,
                                   fragment_root};
        } catch (...) {
            return std::nullopt;
        }
    }

    [[nodiscard]] std::uint64_t generation() const noexcept {
        return read_.generation();
    }

    [[nodiscard]] std::uint64_t semantic_generation() const noexcept {
        return read_.semantic_generation();
    }

    [[nodiscard]] SemanticId node_id() const noexcept { return read_.node_id(); }

    [[nodiscard]] std::optional<VirtualSemanticItemToken>
    virtual_token() const noexcept {
        return read_.virtual_token();
    }

    [[nodiscard]] const SemanticInfo& info() const noexcept { return read_.info(); }

    /// Logical, view-relative bounds. The Win32 boundary applies the T043
    /// scale/screen-origin pair exactly once through `geometry()`.
    [[nodiscard]] Rect logical_bounds() const noexcept {
        return read_.logical_bounds();
    }

    /// Semantic parent identity from the same retained generation. Virtual
    /// logical items report their owning ListView; the root has no parent.
    [[nodiscard]] SemanticId parent_id() const noexcept {
        return read_.parent_id();
    }

    [[nodiscard]] std::size_t child_count() const noexcept {
        return read_.child_count();
    }

    [[nodiscard]] std::optional<SemanticId> child_at(
        std::size_t index) const noexcept {
        return read_.child_at(index);
    }

    [[nodiscard]] std::size_t virtual_child_count() const noexcept {
        return read_.virtual_child_count();
    }

    [[nodiscard]] std::optional<VirtualSemanticItemToken> virtual_child_token_at(
        std::size_t index) const noexcept {
        return read_.virtual_child_token_at(index);
    }

    [[nodiscard]] std::optional<std::size_t> ordinary_child_index_of(
        SemanticId child_id) const noexcept {
        return read_.ordinary_child_index_of(child_id);
    }

    [[nodiscard]] std::optional<std::size_t> virtual_child_index_of(
        VirtualSemanticItemToken token) const {
        return read_.virtual_child_index_of(token);
    }

    [[nodiscard]] std::optional<bool> ordinary_child_selected_at(
        std::size_t index) const noexcept {
        return read_.ordinary_child_selected_at(index);
    }

    [[nodiscard]] std::optional<VirtualSemanticItemToken>
    virtual_selected_child_token() const noexcept {
        return read_.virtual_selected_child_token();
    }

    /// O(1) logical row resolution through the retained fixed-height/scroll
    /// transform. Used by platform hit-testing; never mounts or enumerates
    /// virtual rows.
    [[nodiscard]] std::optional<std::size_t> virtual_child_index_at_logical_y(
        float y) const noexcept {
        return read_.virtual_child_index_at_logical_y(y);
    }

    [[nodiscard]] const SemanticNativeGeometry& geometry() const noexcept {
        return read_.geometry();
    }

    [[nodiscard]] const UiaRoleMapping& role_mapping() const noexcept {
        return role_mapping_;
    }

    [[nodiscard]] const UiaPatternEligibility& patterns() const noexcept {
        return patterns_;
    }

    /// True when this exact generation exposes this identity as the in-view
    /// fragment root (never a virtual logical item).
    [[nodiscard]] bool fragment_root() const noexcept { return fragment_root_; }

private:
    UiaProviderRead(SemanticNativeSnapshotRead read,
                    UiaRoleMapping role_mapping,
                    UiaPatternEligibility patterns,
                    bool fragment_root) noexcept
        : read_(std::move(read)),
          role_mapping_(role_mapping),
          patterns_(patterns),
          fragment_root_(fragment_root) {}

    SemanticNativeSnapshotRead read_;
    UiaRoleMapping role_mapping_{};
    UiaPatternEligibility patterns_{};
    bool fragment_root_{};
};

/// Semantic value domain used to choose the matching UIA property for a
/// committed `ValueChanged` category.
enum class UiaValueKind {
    None,
    Text,
    Numeric,
    Toggle,
    Expanded,
};

[[nodiscard]] inline UiaValueKind uia_value_kind(
    const SemanticInfo& info) noexcept {
    if (info.expanded != SemanticExpandedState::NotApplicable) {
        return UiaValueKind::Expanded;
    }
    if (info.checked != SemanticCheckedState::NotApplicable) {
        return UiaValueKind::Toggle;
    }
    if (info.text_value) {
        return UiaValueKind::Text;
    }
    if (info.numeric_value || info.value_range) {
        return UiaValueKind::Numeric;
    }
    return UiaValueKind::None;
}

/// UIA-facing projection of one committed native publication batch.
///
/// T045's closed change categories deliberately do not carry per-node diff
/// data, so the target identities are resolved from the exact committed
/// generation: structure and bounds target the fragment root, focus targets the
/// currently focused node (falling back to the root so clients can requery a
/// cleared focus), selection targets the selected ordinary or virtual item, and
/// value targets the focused value node, else the single unambiguous value node
/// in the generation, else the focused node/root.
struct UiaEventDerivation final {
    bool structure{};
    bool focus{};
    bool selection{};
    bool value{};
    bool bounds{};
    std::optional<UiaProviderIdentity> structure_target;
    std::optional<UiaProviderIdentity> focus_target;
    std::optional<UiaProviderIdentity> selection_target;
    std::optional<UiaProviderIdentity> value_target;
    std::optional<UiaProviderIdentity> bounds_target;
    UiaValueKind value_kind{UiaValueKind::None};

    bool operator==(const UiaEventDerivation&) const = default;
};

[[nodiscard]] inline UiaEventDerivation derive_uia_events(
    const SemanticNativePublicationBatch& batch) noexcept {
    UiaEventDerivation events;
    try {
        if (!batch.publication || !batch.publication->semantic_snapshot) {
            return events;
        }
        const auto& snapshot = *batch.publication->semantic_snapshot;
        const auto changed = [&batch](SemanticChange change) noexcept {
            return std::find(batch.changes.begin(), batch.changes.end(),
                             change) != batch.changes.end();
        };
        events.structure = changed(SemanticChange::StructureChanged);
        events.focus = changed(SemanticChange::FocusChanged);
        events.selection = changed(SemanticChange::SelectionChanged);
        events.value = changed(SemanticChange::ValueChanged);
        events.bounds = changed(SemanticChange::BoundsChanged);

        const SemanticNodeSnapshot* root = nullptr;
        if (snapshot.root != kInvalidSemanticId) {
            for (const auto& node : snapshot.nodes) {
                if (node.id == snapshot.root) {
                    root = &node;
                    break;
                }
            }
        }
        if (root) {
            const UiaProviderIdentity identity{snapshot.root, std::nullopt};
            events.structure_target = identity;
            events.bounds_target = identity;
        }

        const SemanticNodeSnapshot* focused = nullptr;
        for (const auto& node : snapshot.nodes) {
            if (node.info.focused) {
                focused = &node;
                break;
            }
        }

        if (events.focus) {
            events.focus_target = focused
                ? std::optional<UiaProviderIdentity>{
                      UiaProviderIdentity{focused->id, std::nullopt}}
                : events.structure_target;
        }

        if (events.selection) {
            for (const auto& node : snapshot.nodes) {
                if (node.info.selected) {
                    events.selection_target =
                        UiaProviderIdentity{node.id, std::nullopt};
                    break;
                }
            }
            if (!events.selection_target) {
                for (const auto& node : snapshot.nodes) {
                    if (!node.virtual_children) {
                        continue;
                    }
                    const auto token = node.virtual_children->selected_token();
                    if (token) {
                        events.selection_target =
                            UiaProviderIdentity{node.id, *token};
                        break;
                    }
                }
            }
        }

        if (events.value) {
            const SemanticNodeSnapshot* target = nullptr;
            if (focused && uia_value_kind(focused->info) != UiaValueKind::None) {
                target = focused;
            } else {
                const SemanticNodeSnapshot* single = nullptr;
                std::size_t candidates = 0U;
                for (const auto& node : snapshot.nodes) {
                    if (uia_value_kind(node.info) == UiaValueKind::None) {
                        continue;
                    }
                    single = &node;
                    ++candidates;
                    if (candidates > 1U) {
                        break;
                    }
                }
                if (candidates == 1U) {
                    target = single;
                } else if (focused) {
                    target = focused;
                } else {
                    target = root;
                }
            }
            if (target) {
                events.value_target =
                    UiaProviderIdentity{target->id, std::nullopt};
                events.value_kind = uia_value_kind(target->info);
            }
        }
        return events;
    } catch (...) {
        return UiaEventDerivation{};
    }
}

/// Per-view owner of lazily materialized UIA providers.
///
/// Immutable semantic reads are safe on native accessibility callback threads,
/// and provider materialization/cache bookkeeping is synchronized per view.
/// It owns the native handles it has materialized, and each handle stores only
/// weak publication/action endpoints
/// plus stable semantic identity, so the cache cannot keep a removed semantic
/// snapshot or the view alive. `provider_for()` materializes at most one native
/// provider per requested identity: a 100k-item virtual ListView creates zero
/// providers at publication time and at most one per explicit item query or
/// action. Structure-change handling may call `apply_publication_batch()` to
/// release every currently cached stale identity; a stale query fails closed
/// with no native provider.
class UiaProviderEndpoint final
    : public std::enable_shared_from_this<UiaProviderEndpoint> {
public:
    using Handle = UiaProviderHandlePtr;

    using Factory = Handle (*)(void* user_data,
                               const UiaProviderState& state,
                               const UiaProviderRead& read);

    UiaProviderEndpoint(
        std::weak_ptr<const SemanticNativePublicationSource> publication_source,
        std::weak_ptr<const SemanticActionViewEndpoint> action_endpoint = {}) noexcept
        : publication_source_(std::move(publication_source)),
          action_endpoint_(std::move(action_endpoint)) {}

    /// Retire every materialized handle. Each handle receives exactly one
    /// `on_retired()` notification before the endpoint releases its reference.
    ~UiaProviderEndpoint() noexcept { clear(); }

    UiaProviderEndpoint(const UiaProviderEndpoint&) = delete;
    UiaProviderEndpoint& operator=(const UiaProviderEndpoint&) = delete;
    UiaProviderEndpoint(UiaProviderEndpoint&&) = delete;
    UiaProviderEndpoint& operator=(UiaProviderEndpoint&&) = delete;

    /// Install the native-provider factory. The endpoint never calls it after
    /// destruction or after the publication source expires; `user_data` must
    /// stay valid for the endpoint's lifetime.
    void set_factory(Factory factory, void* user_data) noexcept {
        std::lock_guard lock{mutex_};
        factory_ = factory;
        factory_user_data_ = user_data;
    }

    [[nodiscard]] bool available() const noexcept {
        std::lock_guard lock{mutex_};
        return factory_ != nullptr && !publication_source_.expired();
    }

    /// Resolve one identity against the latest immutable publication without
    /// materializing a native provider. A missing identity, detached view or
    /// flattened role fails closed.
    [[nodiscard]] std::optional<UiaProviderRead> read(
        const UiaProviderIdentity& identity) noexcept {
        const auto state = state_for(identity);
        if (!state) {
            return std::nullopt;
        }
        try {
            return UiaProviderRead::from_state(*state);
        } catch (...) {
            return std::nullopt;
        }
    }

    /// Resolve and lazily materialize one provider for an exact semantic
    /// identity. A missing identity returns an empty handle; an existing live
    /// identity returns the identical cached handle.
    [[nodiscard]] Handle provider_for(
        const UiaProviderIdentity& identity) noexcept {
        if (!identity.valid()) {
            return {};
        }

        // Native accessibility queries may arrive concurrently. Serialize the
        // bounded cache/factory path so one semantic identity materializes one
        // canonical provider. The factory is an internal native-provider
        // constructor and must not re-enter this endpoint.
        std::lock_guard lock{mutex_};
        if (!factory_) {
            return {};
        }
        const auto existing = entries_.find(identity);
        if (existing != entries_.end()) {
            return existing->second;
        }

        const auto state = state_for(identity);
        if (!state) {
            return {};
        }
        const auto read = UiaProviderRead::from_state(*state);
        if (!read) {
            return {};
        }
        return create_and_track_locked(identity, *state, *read);
    }

    /// Materialize the current fragment root. No root identity means no
    /// semantic root has been committed yet (or the view retired), so UIA falls
    /// through to the host window provider.
    [[nodiscard]] Handle root() noexcept {
        const auto publication = current_publication();
        if (!publication || !publication->semantic_snapshot) {
            return {};
        }
        const SemanticId root_id = publication->semantic_snapshot->root;
        if (root_id == kInvalidSemanticId) {
            return {};
        }
        return provider_for(UiaProviderIdentity{root_id, std::nullopt});
    }

    [[nodiscard]] Handle ordinary(SemanticId node_id) noexcept {
        return provider_for(UiaProviderIdentity{node_id, std::nullopt});
    }

    [[nodiscard]] Handle virtual_item(SemanticId list_node_id,
                                      VirtualSemanticItemToken token) noexcept {
        return provider_for(UiaProviderIdentity{list_node_id, token});
    }

    [[nodiscard]] std::optional<std::size_t> ordinary_child_count(
        SemanticId parent_node_id) noexcept {
        const auto read = read_identity(
            UiaProviderIdentity{parent_node_id, std::nullopt});
        return read ? std::optional<std::size_t>{read->child_count()}
                    : std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> virtual_child_count(
        SemanticId list_node_id) noexcept {
        const auto read = read_identity(
            UiaProviderIdentity{list_node_id, std::nullopt});
        return read ? std::optional<std::size_t>{read->virtual_child_count()}
                    : std::nullopt;
    }

    [[nodiscard]] Handle ordinary_child_at(SemanticId parent_node_id,
                                           std::size_t index) noexcept {
        const auto read = read_identity(
            UiaProviderIdentity{parent_node_id, std::nullopt});
        if (!read) {
            return {};
        }
        const auto child_id = read->child_at(index);
        if (!child_id || *child_id == kInvalidSemanticId) {
            return {};
        }
        return provider_for(UiaProviderIdentity{*child_id, std::nullopt});
    }

    [[nodiscard]] Handle virtual_child_at(SemanticId list_node_id,
                                          std::size_t index) noexcept {
        const auto read = read_identity(
            UiaProviderIdentity{list_node_id, std::nullopt});
        if (!read) {
            return {};
        }
        const auto token = read->virtual_child_token_at(index);
        if (!token || *token == kInvalidVirtualSemanticItemToken) {
            return {};
        }
        return provider_for(UiaProviderIdentity{list_node_id, *token});
    }

    /// Resolve one virtual child's stable token from the current publication
    /// without materializing a native provider. Used by lazy item-container
    /// searches, which must never pre-create O(N) providers.
    [[nodiscard]] std::optional<VirtualSemanticItemToken> virtual_child_token_at(
        SemanticId list_node_id,
        std::size_t index) noexcept {
        const auto read = read_identity(
            UiaProviderIdentity{list_node_id, std::nullopt});
        if (!read) {
            return std::nullopt;
        }
        return read->virtual_child_token_at(index);
    }

    [[nodiscard]] std::optional<std::size_t> ordinary_child_index_of(
        SemanticId parent_node_id,
        SemanticId child_node_id) noexcept {
        const auto read = read_identity(
            UiaProviderIdentity{parent_node_id, std::nullopt});
        if (!read) {
            return std::nullopt;
        }
        return read->ordinary_child_index_of(child_node_id);
    }

    [[nodiscard]] std::optional<std::size_t> virtual_child_index_of(
        SemanticId list_node_id,
        VirtualSemanticItemToken token) {
        const auto read = read_identity(
            UiaProviderIdentity{list_node_id, std::nullopt});
        if (!read) {
            return std::nullopt;
        }
        return read->virtual_child_index_of(token);
    }

    /// Resolve the semantic parent identity of one provider. A root or a stale
    /// identity returns an empty handle.
    [[nodiscard]] Handle parent(const UiaProviderIdentity& identity) noexcept {
        const auto read = read_identity(identity);
        if (!read) {
            return {};
        }
        const SemanticId parent_id = read->parent_id();
        if (parent_id == kInvalidSemanticId) {
            return {};
        }
        return provider_for(UiaProviderIdentity{parent_id, std::nullopt});
    }

    /// Materialize at most one selected child. Ordinary children are small and
    /// checked in logical order; virtual collections use the retained selected
    /// token plus the immutable token index, so a 100k-item list never scans or
    /// materializes its dataset.
    [[nodiscard]] Handle selected_child(SemanticId parent_node_id) noexcept {
        const auto read = read_identity(
            UiaProviderIdentity{parent_node_id, std::nullopt});
        if (!read) {
            return {};
        }

        const std::size_t ordinary_count = read->child_count();
        const std::size_t virtual_count = read->virtual_child_count();
        if (ordinary_count != 0U && virtual_count != 0U) {
            return {};
        }

        if (virtual_count != 0U) {
            const auto token = read->virtual_selected_child_token();
            if (!token) {
                return {};
            }
            const auto index = read->virtual_child_index_of(*token);
            if (!index) {
                return {};
            }
            const auto resolved = read->virtual_child_token_at(*index);
            if (!resolved) {
                return {};
            }
            return provider_for(UiaProviderIdentity{parent_node_id, *resolved});
        }

        std::optional<std::size_t> selected_index;
        for (std::size_t index = 0U; index < ordinary_count; ++index) {
            const auto selected = read->ordinary_child_selected_at(index);
            if (!selected) {
                return {};
            }
            if (!*selected) {
                continue;
            }
            if (selected_index) {
                return {};
            }
            selected_index = index;
        }
        if (!selected_index) {
            return {};
        }
        const auto child_id = read->child_at(*selected_index);
        if (!child_id || *child_id == kInvalidSemanticId) {
            return {};
        }
        return provider_for(UiaProviderIdentity{*child_id, std::nullopt});
    }

    [[nodiscard]] std::optional<UiaProviderIdentity> focused_identity()
        const noexcept {
        const auto publication = current_publication();
        if (!publication || !publication->semantic_snapshot) {
            return std::nullopt;
        }
        for (const auto& node : publication->semantic_snapshot->nodes) {
            if (node.info.focused) {
                return UiaProviderIdentity{node.id, std::nullopt};
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<UiaProviderIdentity> selected_identity()
        const noexcept {
        const auto publication = current_publication();
        if (!publication || !publication->semantic_snapshot) {
            return std::nullopt;
        }
        for (const auto& node : publication->semantic_snapshot->nodes) {
            if (node.info.selected) {
                return UiaProviderIdentity{node.id, std::nullopt};
            }
        }
        for (const auto& node : publication->semantic_snapshot->nodes) {
            if (!node.virtual_children) {
                continue;
            }
            const auto token = node.virtual_children->selected_token();
            if (token) {
                return UiaProviderIdentity{node.id, *token};
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] Handle focused() noexcept {
        const auto identity = focused_identity();
        if (!identity) {
            return {};
        }
        return provider_for(*identity);
    }

    [[nodiscard]] std::size_t tracked_identities() const noexcept {
        std::lock_guard lock{mutex_};
        return entries_.size();
    }

    /// Consume one exact committed native publication batch at the cache
    /// boundary. Only a structure change can make a previously materialized
    /// identity disappear, so non-structural batches never scan the cache. A
    /// superseded or foreign batch is ignored: pruning is permitted only when
    /// the batch still is this view's exact current publication.
    [[nodiscard]] std::size_t apply_publication_batch(
        const SemanticNativePublicationBatch& batch) noexcept {
        if (!batch.publication ||
            std::find(batch.changes.begin(), batch.changes.end(),
                      SemanticChange::StructureChanged) == batch.changes.end()) {
            return 0U;
        }
        const auto source = publication_source_.lock();
        if (!source) {
            return 0U;
        }
        const auto current = source->current();
        if (!current || current.get() != batch.publication.get()) {
            return 0U;
        }
        return prune_defunct();
    }

    /// Drop cached providers whose identity is absent from the current
    /// immutable publication. A failed query is not treated as proof of
    /// staleness: the entry is retained and may be retried later. Every removed
    /// handle receives exactly one `on_retired()` notification before the
    /// endpoint releases its reference.
    [[nodiscard]] std::size_t prune_defunct() noexcept {
        std::vector<Handle> retired;
        {
            std::lock_guard lock{mutex_};
            try {
                retired.reserve(entries_.size());
            } catch (...) {
                return 0U;
            }

            for (auto it = entries_.begin(); it != entries_.end();) {
                bool stale = false;
                try {
                    const auto state = state_for(it->first);
                    if (!state) {
                        stale = true;
                    } else {
                        const auto read = UiaProviderRead::from_state(*state);
                        stale = !read.has_value();
                    }
                } catch (...) {
                    stale = false;
                }

                if (!stale) {
                    ++it;
                    continue;
                }

                retired.push_back(std::move(it->second));
                it = entries_.erase(it);
            }
        }

        for (const auto& handle : retired) {
            retire(handle);
        }
        return retired.size();
    }

    void clear() noexcept {
        EntryMap retired;
        {
            std::lock_guard lock{mutex_};
            retired.swap(entries_);
        }
        for (const auto& entry : retired) {
            retire(entry.second);
        }
    }

private:
    using EntryMap =
        std::unordered_map<UiaProviderIdentity, Handle, UiaProviderIdentityHash>;

    [[nodiscard]] std::shared_ptr<const SemanticNativePublicationSnapshot>
    current_publication() const noexcept {
        try {
            const auto source = publication_source_.lock();
            if (!source) {
                return {};
            }
            return source->current();
        } catch (...) {
            return {};
        }
    }

    [[nodiscard]] std::optional<UiaProviderState> state_for(
        const UiaProviderIdentity& identity) const noexcept {
        if (!identity.valid()) {
            return std::nullopt;
        }
        if (identity.virtual_token) {
            return UiaProviderState::virtual_item(
                publication_source_, identity.node_id, *identity.virtual_token,
                action_endpoint_);
        }
        return UiaProviderState::ordinary(publication_source_, identity.node_id,
                                          action_endpoint_);
    }

    [[nodiscard]] std::optional<SemanticNativeSnapshotRead> read_identity(
        const UiaProviderIdentity& identity) const noexcept {
        const auto state = state_for(identity);
        if (!state) {
            return std::nullopt;
        }
        try {
            return state->read();
        } catch (...) {
            return std::nullopt;
        }
    }

    [[nodiscard]] Handle create_and_track_locked(
        const UiaProviderIdentity& identity,
        const UiaProviderState& state,
        const UiaProviderRead& read) noexcept {
        Handle handle;
        try {
            handle = factory_(factory_user_data_, state, read);
        } catch (...) {
            return {};
        }
        if (!handle) {
            return {};
        }
        try {
            entries_.emplace(identity, handle);
        } catch (...) {
            return {};
        }
        return handle;
    }

    static void retire(const Handle& handle) noexcept {
        if (!handle) {
            return;
        }
        try {
            handle->on_retired();
        } catch (...) {
            // Native retirement is best-effort and must never unwind into a
            // platform callback or a destructor.
        }
    }

    std::weak_ptr<const SemanticNativePublicationSource> publication_source_;
    std::weak_ptr<const SemanticActionViewEndpoint> action_endpoint_;
    mutable std::mutex mutex_;
    Factory factory_{};
    void* factory_user_data_{};
    EntryMap entries_;
};

} // namespace ui::detail
