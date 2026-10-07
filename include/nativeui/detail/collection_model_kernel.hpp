#pragma once

#include <cstddef>
#include <cstdint>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace ui::detail {
class CollectionModelError : public std::invalid_argument {
  public:
    using std::invalid_argument::invalid_argument;
};
using CollectionToken = std::uint64_t;
inline constexpr CollectionToken kInvalidCollectionToken = 0;
[[nodiscard]] std::vector<std::size_t>
unique_collection_indices(std::size_t count,
                          const std::function<bool(std::size_t, std::size_t)> &equal);

struct CollectionWriteReceipt {
    bool accepted{};
};
[[nodiscard]] std::shared_ptr<CollectionWriteReceipt> make_collection_write_receipt();
class CollectionKeys {
  public:
    virtual ~CollectionKeys() = default;
    [[nodiscard]] virtual std::size_t size() const noexcept = 0;
    [[nodiscard]] virtual bool equal_at(std::size_t index, const CollectionKeys &other,
                                        std::size_t other_index) const = 0;
    [[nodiscard]] virtual std::optional<std::string> encoded_at(std::size_t index) const = 0;
};
struct CollectionTokenSnapshot {
    std::uint64_t generation{};
    std::shared_ptr<const CollectionKeys> keys;
    std::vector<CollectionToken> tokens;
    std::vector<std::pair<std::string, std::size_t>> encoded_index;
    std::vector<std::pair<CollectionToken, std::size_t>> token_index;
    bool encoded{};
};
struct PreparedCollectionTokens {
    std::uint64_t expected_generation{};
    CollectionToken next_token{1};
    std::shared_ptr<const CollectionTokenSnapshot> snapshot;
};
class CollectionTokens {
  public:
    CollectionTokens();
    CollectionTokens(const CollectionTokens &) = delete;
    CollectionTokens &operator=(const CollectionTokens &) = delete;
    [[nodiscard]] PreparedCollectionTokens
    prepare(std::shared_ptr<const CollectionKeys> keys) const;
    [[nodiscard]] bool commit(PreparedCollectionTokens prepared) noexcept;
    [[nodiscard]] const std::shared_ptr<const CollectionTokenSnapshot> &snapshot() const noexcept {
        return snapshot_;
    }
    [[nodiscard]] static std::optional<std::size_t>
    index_for_token(const CollectionTokenSnapshot &snapshot, CollectionToken token) noexcept;

  private:
    std::shared_ptr<const CollectionTokenSnapshot> snapshot_;
    CollectionToken next_token_{1};
};

struct CollectionRange {
    std::size_t first{};
    std::size_t last{};
    bool operator==(const CollectionRange &) const = default;
};
struct CollectionHeightNode;
// Immutable geometry for semantic/read snapshots. Corrections path-copy the
// index; old snapshots retain their bounds without copying the logical rows.
class CollectionHeightSnapshot {
  public:
    CollectionHeightSnapshot() = default;
    [[nodiscard]] std::size_t size() const noexcept { return count_; }
    [[nodiscard]] double total_height() const noexcept { return total_; }
    [[nodiscard]] double height_at(std::size_t index) const noexcept;
    [[nodiscard]] double prefix_sum(std::size_t end) const noexcept;
    [[nodiscard]] std::optional<std::size_t> index_at(double offset) const noexcept;
    [[nodiscard]] CollectionRange range(double offset, double viewport,
                                        std::size_t overscan) const noexcept;

  private:
    friend class CollectionHeightIndex;
    CollectionHeightSnapshot(std::shared_ptr<const CollectionHeightNode> root, std::size_t count,
                             std::size_t leaf_base, double total)
        : root_(std::move(root)), count_(count), leaf_base_(leaf_base), total_(total) {}
    std::shared_ptr<const CollectionHeightNode> root_;
    std::size_t count_{};
    std::size_t leaf_base_{1};
    double total_{};
};
// Positive logical row heights, independent of user keys and retained widgets.
// Dataset publication rebuilds once; ordinary queries and one measured-height
// correction walk the prefix index without scanning the logical dataset.
class CollectionHeightIndex {
  public:
    void replace(std::vector<double> heights);
    [[nodiscard]] bool set_height(std::size_t index, double height);
    [[nodiscard]] std::size_t size() const noexcept { return heights_.size(); }
    [[nodiscard]] double height_at(std::size_t index) const noexcept;
    [[nodiscard]] double total_height() const noexcept { return total_; }
    [[nodiscard]] CollectionHeightSnapshot snapshot() const noexcept {
        return {root_, heights_.size(), leaf_base_, total_};
    }
    [[nodiscard]] double prefix_sum(std::size_t end) const noexcept;
    [[nodiscard]] std::optional<std::size_t> index_at(double offset) const noexcept;
    [[nodiscard]] CollectionRange range(double offset, double viewport,
                                        std::size_t overscan) const noexcept;

  private:
    std::shared_ptr<const CollectionHeightNode> root_;
    std::vector<double> heights_;
    std::vector<double> prefix_;
    double total_{};
    std::size_t leaf_base_{1};
};

struct CollectionSearchItem {
    std::string label;
    bool eligible{true};
};
// Committed UTF-8 only. The owner supplies its own UI clock/timer and clears
// this buffer on Escape, unavailable state or teardown. Search is data-only.
class CollectionTypeahead {
  public:
    using Time = std::chrono::milliseconds;
    [[nodiscard]] std::optional<std::size_t> feed(std::string_view text, Time now,
                                                  const std::vector<CollectionSearchItem> &items,
                                                  std::optional<std::size_t> active);
    void clear() noexcept;
    [[nodiscard]] const std::string &buffer() const noexcept { return buffer_; }

  private:
    std::string buffer_;
    Time last_{};
    bool timestamp_valid_{};
};
enum class CollectionSelectionGesture { Replace, Toggle, Extend, AddRange, MoveActive, SelectAll };
struct CollectionSelection {
    std::vector<CollectionToken> selected;
    std::optional<CollectionToken> active;
    std::optional<CollectionToken> anchor;
    bool operator==(const CollectionSelection &) const = default;
};
[[nodiscard]] CollectionSelection
resolve_collection_selection(const std::vector<CollectionToken> &logical_order,
                             const std::vector<bool> &eligible, const CollectionSelection &current,
                             bool multiple, std::optional<CollectionToken> target,
                             CollectionSelectionGesture gesture);
} // namespace ui::detail
