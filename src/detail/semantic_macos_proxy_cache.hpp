#pragma once

#if !defined(__OBJC__)
#error "semantic_macos_proxy_cache.hpp requires Objective-C++"
#endif

#include "semantic_macos_appkit.hpp"
#include "semantic_macos_children.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ui::detail {

struct MacOSAccessibilityChildIdentityHash final {
    [[nodiscard]] std::size_t operator()(
        const MacOSAccessibilityChildIdentity& identity) const noexcept {
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

/// Shared per-native-view endpoint for lazily materialized macOS accessibility
/// elements.
///
/// The endpoint is intentionally AppKit/main-thread confined because creation of
/// a runtime accessibility element crosses the Objective-C/AppKit boundary. It
/// owns the native elements it has materialized, but each element stores only a
/// weak publication source, a weak reference back to this child resolver, and
/// stable semantic identity. The resulting graph cannot keep the endpoint or
/// removed semantic snapshot content alive through cached proxies. A stale
/// identity is evicted when queried and structure-change handling may call
/// prune_defunct() to release all currently cached stale elements.
///
/// The owning view keeps this endpoint strongly through MacOSAccessibilityProxyCache.
/// Native proxy callbacks retain only a weak endpoint reference and may lock it
/// for one callback. This avoids a raw pointer back to the cache while also
/// avoiding an ownership cycle between cached elements and their resolver.
///
/// Virtual collection access remains lazy: one lookup can create at most one
/// NSAccessibilityElement, independently of the logical collection size.
class MacOSAccessibilityProxyCacheEndpoint final
    : public MacOSAccessibilityChildResolver,
      public std::enable_shared_from_this<MacOSAccessibilityProxyCacheEndpoint> {
public:
    using ElementRange = MacOSAccessibilityChildResolver::ElementRange;

    MacOSAccessibilityProxyCacheEndpoint(
        Class consumer_view_class,
        std::weak_ptr<const SemanticNativePublicationSource> publication_source) noexcept
        : consumer_view_class_(consumer_view_class),
          publication_source_(std::move(publication_source)) {}

    ~MacOSAccessibilityProxyCacheEndpoint() noexcept override {
        clear();
    }

    MacOSAccessibilityProxyCacheEndpoint(
        const MacOSAccessibilityProxyCacheEndpoint&) = delete;
    MacOSAccessibilityProxyCacheEndpoint& operator=(
        const MacOSAccessibilityProxyCacheEndpoint&) = delete;
    MacOSAccessibilityProxyCacheEndpoint(
        MacOSAccessibilityProxyCacheEndpoint&&) = delete;
    MacOSAccessibilityProxyCacheEndpoint& operator=(
        MacOSAccessibilityProxyCacheEndpoint&&) = delete;

    [[nodiscard]] NSAccessibilityElement* ordinary(SemanticId node_id) noexcept {
        return get_or_create(
            MacOSAccessibilityChildIdentity{node_id, std::nullopt});
    }

    [[nodiscard]] NSAccessibilityElement* virtual_item(
        SemanticId list_node_id,
        VirtualSemanticItemToken token) noexcept {
        return get_or_create(
            MacOSAccessibilityChildIdentity{list_node_id, token});
    }

    /// Count the children visible through AppKit from one exact immutable
    /// publication. Ordinary semantic children and virtual collection children
    /// are mutually exclusive in the frozen model; if a malformed snapshot
    /// exposes both domains at once, fail closed instead of inventing an order.
    [[nodiscard]] std::optional<std::size_t> appkit_child_count(
        SemanticId parent_node_id) noexcept override {
        auto projection = child_projection(parent_node_id);
        if (!projection) {
            return std::nullopt;
        }

        const std::size_t ordinary_count = projection->ordinary_child_count();
        const std::size_t virtual_count = projection->virtual_child_count();
        if (ordinary_count != 0U && virtual_count != 0U) {
            return std::nullopt;
        }
        return virtual_count != 0U ? virtual_count : ordinary_count;
    }

    /// Resolve one AppKit-requested child window from one exact immutable
    /// publication. Only the requested proxies are materialized. A mixed
    /// ordinary+virtual domain fails closed rather than guessing platform order.
    [[nodiscard]] std::optional<ElementRange> appkit_children_range(
        SemanticId parent_node_id,
        std::size_t start,
        std::size_t max_count) noexcept override {
        auto projection = child_projection(parent_node_id);
        if (!projection) {
            return std::nullopt;
        }

        const std::size_t ordinary_count = projection->ordinary_child_count();
        const std::size_t virtual_count = projection->virtual_child_count();
        if (ordinary_count != 0U && virtual_count != 0U) {
            return std::nullopt;
        }

        try {
            return materialize_children_range(
                *projection,
                start,
                max_count,
                virtual_count != 0U);
        } catch (...) {
            return std::nullopt;
        }
    }

    /// Materialize the ordinary semantic parent described by one callback-local
    /// child projection. Parent identity and initial role are both read from the
    /// exact immutable publication retained by that projection, so a cache miss
    /// never reloads a successor generation before constructing the parent proxy.
    /// Virtual rows naturally resolve to their owning ListView as an ordinary
    /// parent; roots and unmapped parents fail closed.
    [[nodiscard]] NSAccessibilityElement* parent_from_projection(
        const MacOSAccessibilityChildProjection& child_projection) noexcept {
        try {
            const auto identity = child_projection.parent_identity();
            const auto semantic_role = child_projection.parent_role();
            if (!identity || identity->virtual_token.has_value() || !semantic_role) {
                return nil;
            }

            const auto initial_mapping =
                macos_accessibility_role_mapping(*semantic_role);
            if (!initial_mapping) {
                return nil;
            }
            return get_or_create_known_live(*identity, *initial_mapping);
        } catch (...) {
            return nil;
        }
    }

    /// Count ordinary children from one immutable publication without creating
    /// native proxy objects. This is the O(1) count primitive for AppKit's
    /// bounded array-attribute query path.
    [[nodiscard]] std::optional<std::size_t> ordinary_child_count(
        SemanticId parent_node_id) noexcept {
        return child_count(parent_node_id, false);
    }

    /// Count virtual children from shared immutable metadata. The logical
    /// collection size is returned without visiting or materializing its rows.
    [[nodiscard]] std::optional<std::size_t> virtual_child_count(
        SemanticId list_node_id) noexcept {
        return child_count(list_node_id, true);
    }

    /// Resolve one ordinary child from one exact immutable publication read.
    /// This path is bounded and creates at most the requested native element.
    [[nodiscard]] NSAccessibilityElement* ordinary_child_at(
        SemanticId parent_node_id,
        std::size_t index) noexcept {
        return indexed_child_at(parent_node_id, index, false);
    }

    /// Resolve one virtual child by immutable index without enumerating or
    /// materializing the complete logical collection.
    [[nodiscard]] NSAccessibilityElement* virtual_child_at(
        SemanticId list_node_id,
        std::size_t index) noexcept {
        return indexed_child_at(list_node_id, index, true);
    }

    /// Resolve a requested ordinary-child window from one immutable publication.
    /// Work and native proxy creation are O(returned count), never O(total count).
    [[nodiscard]] std::optional<ElementRange> ordinary_children_range(
        SemanticId parent_node_id,
        std::size_t start,
        std::size_t max_count) noexcept {
        return children_range(parent_node_id, start, max_count, false);
    }

    /// Resolve a requested virtual-child window without walking the full logical
    /// dataset. This is the bounded primitive used by native array-range queries.
    [[nodiscard]] std::optional<ElementRange> virtual_children_range(
        SemanticId list_node_id,
        std::size_t start,
        std::size_t max_count) noexcept {
        return children_range(list_node_id, start, max_count, true);
    }

    [[nodiscard]] std::size_t tracked_identities() const noexcept {
        return entries_.size();
    }

    /// Drop cached elements whose identity is absent from the current immutable
    /// publication. Allocation/query failure is not treated as proof of
    /// staleness: the entry is retained and may be retried on a later checkpoint.
    [[nodiscard]] std::size_t prune_defunct() noexcept {
        if (![NSThread isMainThread]) {
            return 0U;
        }

        std::size_t erased = 0U;
        for (auto it = entries_.begin(); it != entries_.end();) {
            const auto state = state_for(it->first);
            bool stale = !state.has_value();
            if (!stale) {
                try {
                    stale = !macos_accessibility_proxy_read(*state).has_value();
                } catch (...) {
                    ++it;
                    continue;
                }
            }

            if (!stale) {
                ++it;
                continue;
            }

            NSAccessibilityElement* const element = it->second;
            it = entries_.erase(it);
            release_element(element);
            ++erased;
        }
        return erased;
    }

    void clear() noexcept {
        while (!entries_.empty()) {
            const auto it = entries_.begin();
            NSAccessibilityElement* const element = it->second;
            entries_.erase(it);
            release_element(element);
        }
    }

private:
    using EntryMap = std::unordered_map<
        MacOSAccessibilityChildIdentity,
        NSAccessibilityElement*,
        MacOSAccessibilityChildIdentityHash>;

    [[nodiscard]] std::optional<MacOSAccessibilityProxyState> state_for(
        const MacOSAccessibilityChildIdentity& identity) noexcept {
        if (!identity.valid()) {
            return std::nullopt;
        }

        const std::weak_ptr<MacOSAccessibilityChildResolver> child_resolver =
            weak_from_this();
        if (identity.virtual_token) {
            return MacOSAccessibilityProxyState::virtual_item(
                publication_source_,
                identity.node_id,
                *identity.virtual_token,
                child_resolver);
        }
        return MacOSAccessibilityProxyState::ordinary(
            publication_source_, identity.node_id, child_resolver);
    }

    [[nodiscard]] NSAccessibilityElement* tracked_element(
        const MacOSAccessibilityChildIdentity& identity) const noexcept {
        const auto existing = entries_.find(identity);
        return existing != entries_.end() ? existing->second : nil;
    }

    [[nodiscard]] std::optional<MacOSAccessibilityChildProjection> child_projection(
        SemanticId parent_node_id) noexcept {
        if (![NSThread isMainThread] || !consumer_view_class_ ||
            parent_node_id == kInvalidSemanticId) {
            return std::nullopt;
        }

        try {
            auto parent_state = MacOSAccessibilityProxyState::ordinary(
                publication_source_, parent_node_id);
            if (!parent_state) {
                return std::nullopt;
            }

            auto read = parent_state->read();
            if (!read || !macos_accessibility_role_mapping(read->info().role)) {
                return std::nullopt;
            }
            return MacOSAccessibilityChildProjection{std::move(*read)};
        } catch (...) {
            return std::nullopt;
        }
    }

    [[nodiscard]] std::optional<std::size_t> child_count(
        SemanticId parent_node_id,
        bool virtual_children) noexcept {
        auto projection = child_projection(parent_node_id);
        if (!projection) {
            return std::nullopt;
        }
        return virtual_children
            ? projection->virtual_child_count()
            : projection->ordinary_child_count();
    }

    [[nodiscard]] NSAccessibilityElement* indexed_child_at(
        SemanticId parent_node_id,
        std::size_t index,
        bool virtual_child) noexcept {
        auto projection = child_projection(parent_node_id);
        if (!projection) {
            return nil;
        }

        try {
            const auto identity = virtual_child
                ? projection->virtual_child_identity_at(index)
                : projection->ordinary_child_identity_at(index);
            if (!identity) {
                return nil;
            }

            if (NSAccessibilityElement* const existing = tracked_element(*identity)) {
                return existing;
            }

            const auto semantic_role = virtual_child
                ? projection->virtual_child_role_at(index)
                : projection->ordinary_child_role_at(index);
            if (!semantic_role) {
                return nil;
            }
            const auto initial_mapping =
                macos_accessibility_role_mapping(*semantic_role);
            if (!initial_mapping) {
                return nil;
            }

            // Identity and initial role both came from the exact publication
            // retained by this callback. Cache-miss construction must not reload
            // a newer generation; the proxy may become defunct afterwards.
            return get_or_create_known_live(*identity, *initial_mapping);
        } catch (...) {
            return nil;
        }
    }

    [[nodiscard]] std::optional<ElementRange> materialize_children_range(
        const MacOSAccessibilityChildProjection& projection,
        std::size_t start,
        std::size_t max_count,
        bool virtual_children) {
        const std::size_t total = virtual_children
            ? projection.virtual_child_count()
            : projection.ordinary_child_count();

        ElementRange result;
        if (max_count == 0U || start >= total) {
            return result;
        }

        const std::size_t count = std::min(max_count, total - start);
        result.reserve(count);
        for (std::size_t offset = 0U; offset < count; ++offset) {
            const std::size_t index = start + offset;
            const auto identity = virtual_children
                ? projection.virtual_child_identity_at(index)
                : projection.ordinary_child_identity_at(index);
            if (!identity) {
                return std::nullopt;
            }

            if (NSAccessibilityElement* const existing = tracked_element(*identity)) {
                result.push_back(existing);
                continue;
            }

            const auto semantic_role = virtual_children
                ? projection.virtual_child_role_at(index)
                : projection.ordinary_child_role_at(index);
            if (!semantic_role) {
                return std::nullopt;
            }
            const auto initial_mapping =
                macos_accessibility_role_mapping(*semantic_role);
            if (!initial_mapping) {
                return std::nullopt;
            }

            NSAccessibilityElement* const element =
                get_or_create_known_live(*identity, *initial_mapping);
            if (!element) {
                return std::nullopt;
            }
            result.push_back(element);
        }
        return result;
    }

    [[nodiscard]] std::optional<ElementRange> children_range(
        SemanticId parent_node_id,
        std::size_t start,
        std::size_t max_count,
        bool virtual_children) noexcept {
        auto projection = child_projection(parent_node_id);
        if (!projection) {
            return std::nullopt;
        }

        try {
            return materialize_children_range(
                *projection, start, max_count, virtual_children);
        } catch (...) {
            return std::nullopt;
        }
    }

    [[nodiscard]] NSAccessibilityElement* get_or_create(
        const MacOSAccessibilityChildIdentity& identity) noexcept {
        if (![NSThread isMainThread] || !consumer_view_class_) {
            return nil;
        }

        auto state = state_for(identity);
        if (!state) {
            return nil;
        }

        std::optional<MacOSAccessibilityProxyRead> initial_read;
        try {
            initial_read = macos_accessibility_proxy_read(*state);
        } catch (...) {
            return nil;
        }

        auto existing = entries_.find(identity);
        if (!initial_read) {
            if (existing != entries_.end()) {
                NSAccessibilityElement* const element = existing->second;
                entries_.erase(existing);
                release_element(element);
            }
            return nil;
        }
        if (existing != entries_.end()) {
            return existing->second;
        }

        return create_and_track(
            identity, std::move(*state), initial_read->role_mapping());
    }

    [[nodiscard]] NSAccessibilityElement* get_or_create_known_live(
        const MacOSAccessibilityChildIdentity& identity,
        MacOSAccessibilityRoleMapping initial_mapping) noexcept {
        if (![NSThread isMainThread] || !consumer_view_class_) {
            return nil;
        }

        const auto existing = entries_.find(identity);
        if (existing != entries_.end()) {
            return existing->second;
        }

        auto state = state_for(identity);
        if (!state) {
            return nil;
        }
        return create_and_track(
            identity, std::move(*state), initial_mapping);
    }

    [[nodiscard]] NSAccessibilityElement* create_and_track(
        const MacOSAccessibilityChildIdentity& identity,
        MacOSAccessibilityProxyState state,
        MacOSAccessibilityRoleMapping initial_mapping) noexcept {
        NSAccessibilityElement* const created =
            macos_accessibility_appkit_proxy_create(
                consumer_view_class_, std::move(state), initial_mapping);
        if (!created) {
            return nil;
        }

        if (!retain_element(created)) {
            return nil;
        }

        try {
            entries_.emplace(identity, created);
        } catch (...) {
            release_element(created);
            return nil;
        }
        return created;
    }

    [[nodiscard]] static bool retain_element(NSAccessibilityElement* element) noexcept {
        if (!element) {
            return false;
        }
        @try {
            [element retain];
            return true;
        } @catch (...) {
            return false;
        }
    }

    static void release_element(NSAccessibilityElement* element) noexcept {
        if (!element) {
            return;
        }
        @try {
            [element release];
        } @catch (...) {
            // Native teardown is best-effort and no Objective-C exception may
            // escape a NativeUI destructor or accessibility callback boundary.
        }
    }

    Class consumer_view_class_{Nil};
    std::weak_ptr<const SemanticNativePublicationSource> publication_source_;
    EntryMap entries_;
};

/// Owning facade for one per-view macOS accessibility cache endpoint.
///
/// The facade deliberately contains no raw self pointer that could be embedded in
/// an NSAccessibilityElement. Callers that need to hand child-resolution access
/// to a proxy obtain endpoint() and store it weakly. If the view/cache is retired,
/// the weak endpoint expires; if a callback already locked it, the endpoint stays
/// alive only for that callback and remains independent of the facade object.
class MacOSAccessibilityProxyCache final {
public:
    using ElementRange = MacOSAccessibilityProxyCacheEndpoint::ElementRange;

    MacOSAccessibilityProxyCache(
        Class consumer_view_class,
        std::weak_ptr<const SemanticNativePublicationSource> publication_source)
        : endpoint_(std::make_shared<MacOSAccessibilityProxyCacheEndpoint>(
              consumer_view_class, std::move(publication_source))) {}

    ~MacOSAccessibilityProxyCache() noexcept = default;

    MacOSAccessibilityProxyCache(const MacOSAccessibilityProxyCache&) = delete;
    MacOSAccessibilityProxyCache& operator=(const MacOSAccessibilityProxyCache&) = delete;
    MacOSAccessibilityProxyCache(MacOSAccessibilityProxyCache&&) = delete;
    MacOSAccessibilityProxyCache& operator=(MacOSAccessibilityProxyCache&&) = delete;

    [[nodiscard]] std::weak_ptr<MacOSAccessibilityProxyCacheEndpoint>
    endpoint() const noexcept {
        return endpoint_;
    }

    [[nodiscard]] NSAccessibilityElement* ordinary(SemanticId node_id) noexcept {
        return endpoint_->ordinary(node_id);
    }

    [[nodiscard]] NSAccessibilityElement* virtual_item(
        SemanticId list_node_id,
        VirtualSemanticItemToken token) noexcept {
        return endpoint_->virtual_item(list_node_id, token);
    }

    [[nodiscard]] std::optional<std::size_t> ordinary_child_count(
        SemanticId parent_node_id) noexcept {
        return endpoint_->ordinary_child_count(parent_node_id);
    }

    [[nodiscard]] std::optional<std::size_t> virtual_child_count(
        SemanticId list_node_id) noexcept {
        return endpoint_->virtual_child_count(list_node_id);
    }

    [[nodiscard]] NSAccessibilityElement* ordinary_child_at(
        SemanticId parent_node_id,
        std::size_t index) noexcept {
        return endpoint_->ordinary_child_at(parent_node_id, index);
    }

    [[nodiscard]] NSAccessibilityElement* virtual_child_at(
        SemanticId list_node_id,
        std::size_t index) noexcept {
        return endpoint_->virtual_child_at(list_node_id, index);
    }

    [[nodiscard]] std::optional<ElementRange> ordinary_children_range(
        SemanticId parent_node_id,
        std::size_t start,
        std::size_t max_count) noexcept {
        return endpoint_->ordinary_children_range(parent_node_id, start, max_count);
    }

    [[nodiscard]] std::optional<ElementRange> virtual_children_range(
        SemanticId list_node_id,
        std::size_t start,
        std::size_t max_count) noexcept {
        return endpoint_->virtual_children_range(list_node_id, start, max_count);
    }

    [[nodiscard]] std::size_t tracked_identities() const noexcept {
        return endpoint_->tracked_identities();
    }

    [[nodiscard]] std::size_t prune_defunct() noexcept {
        return endpoint_->prune_defunct();
    }

    void clear() noexcept {
        endpoint_->clear();
    }

private:
    std::shared_ptr<MacOSAccessibilityProxyCacheEndpoint> endpoint_;
};

} // namespace ui::detail
