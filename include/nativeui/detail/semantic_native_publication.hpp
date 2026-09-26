#pragma once

#include <nativeui/semantics.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include <vector>

namespace ui::detail {

/// Small immutable native-coordinate state captured on the owning UI thread.
///
/// SemanticTreeSnapshot deliberately stays logical/view-relative. Platform
/// readers receive this copied geometry next to the exact immutable semantic
/// snapshot, so they never need to inspect live ViewGeometryState cross-thread.
struct SemanticNativeGeometry final {
    float scale{1.0f};
    Point physical_screen_origin{};

    [[nodiscard]] bool operator==(const SemanticNativeGeometry& other) const noexcept {
        return scale == other.scale &&
               physical_screen_origin.x == other.physical_screen_origin.x &&
               physical_screen_origin.y == other.physical_screen_origin.y;
    }
};

/// One immutable native-reader publication.
///
/// `generation` is a native publication generation, distinct from the logical
/// semantic generation. It advances when the closed notification set changes,
/// including a geometry-only BoundsChanged publication, while the referenced
/// SemanticTreeSnapshot keeps the semantic-generation contract unchanged.
struct SemanticNativePublicationSnapshot final {
    std::shared_ptr<const SemanticTreeSnapshot> semantic_snapshot;
    SemanticNativeGeometry geometry;
    std::uint64_t generation{};

    [[nodiscard]] std::uint64_t semantic_generation() const noexcept {
        return semantic_snapshot ? semantic_snapshot->generation : 0;
    }
};

struct SemanticNativePublicationBatch final {
    std::shared_ptr<const SemanticNativePublicationSnapshot> publication;
    std::vector<SemanticChange> changes;

    [[nodiscard]] std::uint64_t generation() const noexcept {
        return publication ? publication->generation : 0;
    }

    [[nodiscard]] std::uint64_t semantic_generation() const noexcept {
        return publication ? publication->semantic_generation() : 0;
    }
};

class SemanticNativePublicationState;

/// Lifetime-safe read endpoint for native accessibility proxies.
///
/// One publication owner keeps this source alive for exactly one native view.
/// Native proxies retain only a weak reference to the source. Shutdown clears
/// the atomically published generation before retained/native teardown, while a
/// callback that already loaded one immutable publication may finish reading it
/// through normal shared ownership. The source exposes no mutation API.
class SemanticNativePublicationSource final {
public:
    [[nodiscard]] std::shared_ptr<const SemanticNativePublicationSnapshot>
    current() const noexcept {
#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
        return current_.load(std::memory_order_acquire);
#else
        return std::atomic_load_explicit(&current_, std::memory_order_acquire);
#endif
    }

private:
    friend class SemanticNativePublicationState;

    void store(std::shared_ptr<const SemanticNativePublicationSnapshot> published) noexcept {
#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
        current_.store(std::move(published), std::memory_order_release);
#else
        std::atomic_store_explicit(&current_, std::move(published), std::memory_order_release);
#endif
    }

#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
    std::atomic<std::shared_ptr<const SemanticNativePublicationSnapshot>> current_;
#else
    mutable std::shared_ptr<const SemanticNativePublicationSnapshot> current_;
#endif
};

/// Per-view immutable publication owner for platform accessibility readers.
///
/// There is one UI-thread writer. Native callback threads only call current()
/// through the lifetime-safe source and retain the returned immutable object.
/// Publication is prepare/commit: all fallible vector/allocation work completes
/// before the atomic store, so failure leaves the previous native generation
/// authoritative.
class SemanticNativePublicationState final {
public:
    enum class FailurePointForTest {
        None,
        PublicationAllocation,
    };

    SemanticNativePublicationState()
        : source_(std::make_shared<SemanticNativePublicationSource>()) {}

    SemanticNativePublicationState(const SemanticNativePublicationState&) = delete;
    SemanticNativePublicationState& operator=(const SemanticNativePublicationState&) = delete;
    SemanticNativePublicationState(SemanticNativePublicationState&&) = delete;
    SemanticNativePublicationState& operator=(SemanticNativePublicationState&&) = delete;

    [[nodiscard]] std::shared_ptr<const SemanticNativePublicationSnapshot>
    current() const noexcept {
        return source_->current();
    }

    [[nodiscard]] std::weak_ptr<const SemanticNativePublicationSource>
    reader_source() const noexcept {
        return source_;
    }

    void shutdown() noexcept {
        retired_ = true;
        source_->store(nullptr);
    }

    void fail_next_publish_at_for_test(FailurePointForTest point) noexcept {
        failure_point_for_test_ = point;
    }

    [[nodiscard]] std::optional<SemanticNativePublicationBatch> publish(
        std::shared_ptr<const SemanticTreeSnapshot> semantic_snapshot,
        const std::vector<SemanticChange>& semantic_changes,
        SemanticNativeGeometry geometry) {
        if (retired_ || !semantic_snapshot) {
            return std::nullopt;
        }

        const auto previous = current();
        auto changes = semantic_changes;
        const bool geometry_changed =
            previous && !(previous->geometry == geometry);

        // Geometry is not an exposed semantic change until a live root exists.
        // Initial root publication remains StructureChanged-only, matching the
        // semantic diff contract; later transform-only changes become exactly
        // one BoundsChanged notification for the resulting native generation.
        if (geometry_changed && semantic_snapshot->root != kInvalidSemanticId &&
            std::find(changes.begin(), changes.end(), SemanticChange::BoundsChanged) ==
                changes.end()) {
            changes.push_back(SemanticChange::BoundsChanged);
        }

        const bool same_semantic_snapshot =
            previous && previous->semantic_snapshot.get() == semantic_snapshot.get();
        if (previous && same_semantic_snapshot && !geometry_changed && changes.empty()) {
            return SemanticNativePublicationBatch{previous, {}};
        }

        std::uint64_t generation = previous ? previous->generation : 0;
        if (!changes.empty()) {
            ++generation;
        }

        fail_if_requested_for_test(FailurePointForTest::PublicationAllocation);
        auto prepared = std::make_shared<const SemanticNativePublicationSnapshot>(
            SemanticNativePublicationSnapshot{
                std::move(semantic_snapshot),
                geometry,
                generation,
            });
        source_->store(prepared);
        return SemanticNativePublicationBatch{
            std::move(prepared),
            std::move(changes),
        };
    }

private:
    void fail_if_requested_for_test(FailurePointForTest point) {
        if (failure_point_for_test_ != point) {
            return;
        }
        failure_point_for_test_ = FailurePointForTest::None;
        throw std::bad_alloc{};
    }

    std::shared_ptr<SemanticNativePublicationSource> source_;
    FailurePointForTest failure_point_for_test_{FailurePointForTest::None};
    bool retired_{};
};

} // namespace ui::detail
