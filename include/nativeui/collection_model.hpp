#pragma once

#include <nativeui/state.hpp>
#include <nativeui/detail/collection_model_kernel.hpp>
#include <nativeui/detail/dynamic_key.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace ui {
enum class SelectionMode { Single, Multiple };
template <class Key> struct SelectionSnapshot {
    std::vector<Key> selected;
    std::optional<Key> active;
    std::optional<Key> anchor;
    bool operator==(const SelectionSnapshot &) const = default;
};
template <class Key> struct CollectionItem {
    Key key;
    std::string label;
    bool enabled{true};
    bool section_header{};
    bool operator==(const CollectionItem &) const = default;
};
template <class Key> struct TreeNode {
    Key key;
    std::optional<Key> parent;
    std::string label;
    bool enabled{true};
    bool branch{};
    bool operator==(const TreeNode &) const = default;
};
struct ListRowHeights {
    double estimate{24.0};
    bool variable{true};
};

template <class Key> class Selection {
  public:
    explicit Selection(Binding<SelectionSnapshot<Key>> source) : source_(std::move(source)) {}
    explicit Selection(State<SelectionSnapshot<Key>> &source) : Selection(source.binding()) {}
    [[nodiscard]] Binding<SelectionSnapshot<Key>> binding() const { return source_; }
    [[nodiscard]] SelectionSnapshot<Key> snapshot() const {
        const auto source = source_;
        return source.snapshot();
    }
    [[nodiscard]] bool valid() const noexcept { return source_.valid(); }
    bool set(SelectionSnapshot<Key> candidate) {
        auto source = source_;
        if (!source.valid())
            return false;
        const auto revision = source.revision();
        const auto indices = detail::unique_collection_indices(
            candidate.selected.size(), [&candidate](std::size_t first, std::size_t second) {
                return candidate.selected[first] == candidate.selected[second];
            });
        if (indices.size() != candidate.selected.size()) {
            std::vector<Key> canonical;
            canonical.reserve(indices.size());
            for (const auto index : indices)
                canonical.push_back(candidate.selected[index]);
            candidate.selected.swap(canonical);
        }
        if (!source.valid() || source.revision() != revision)
            return false;
        const auto current = source.snapshot();
        if (!source.valid() || source.revision() != revision)
            return false;
        const bool same = candidate == current;
        if (same || !source.valid() || source.revision() != revision)
            return false;
        const auto receipt = detail::make_collection_write_receipt();
        source.set_if(std::move(candidate), [source, revision, receipt] {
            if (!source.valid() || source.revision() != revision)
                return false;
            receipt->accepted = true;
            return true;
        });
        // A recursive queued write is not yet committed at the caller's return.
        // The receipt remains owned by its guard until the pass accepts/rejects it.
        return receipt->accepted;
    }

  private:
    Binding<SelectionSnapshot<Key>> source_;
};
namespace detail {
template <class Key> class TypedCollectionKeys final : public CollectionKeys {
  public:
    explicit TypedCollectionKeys(std::vector<Key> keys) : keys_(std::move(keys)) {}
    [[nodiscard]] std::size_t size() const noexcept override { return keys_.size(); }
    [[nodiscard]] bool equal_at(std::size_t index, const CollectionKeys &other,
                                std::size_t other_index) const override {
        const auto *typed = dynamic_cast<const TypedCollectionKeys *>(&other);
        return typed && index < keys_.size() && other_index < typed->keys_.size() &&
               keys_[index] == typed->keys_[other_index];
    }
    [[nodiscard]] std::optional<std::string> encoded_at(std::size_t index) const override {
        if (index >= keys_.size())
            return {};
        if constexpr (std::is_integral_v<Key> || std::is_enum_v<Key> ||
                      std::is_same_v<Key, std::string> || std::is_same_v<Key, std::string_view>)
            return encode_dynamic_key(keys_[index]);
        else
            return {};
    }
    [[nodiscard]] const Key *key_at(std::size_t index) const noexcept {
        return index < keys_.size() ? &keys_[index] : nullptr;
    }

  private:
    std::vector<Key> keys_;
};
template <class Key>
[[nodiscard]] std::optional<std::size_t>
collection_index_of_key(std::shared_ptr<const CollectionTokenSnapshot> snapshot, const Key &key) {
    if (!snapshot)
        return {};
    const auto *typed = dynamic_cast<const TypedCollectionKeys<Key> *>(snapshot->keys.get());
    if (!typed)
        return {};
    if constexpr (std::is_integral_v<Key> || std::is_enum_v<Key> ||
                  std::is_same_v<Key, std::string> || std::is_same_v<Key, std::string_view>) {
        if (snapshot->encoded) {
            const auto encoded = encode_dynamic_key(key);
            const auto found = std::lower_bound(
                snapshot->encoded_index.begin(), snapshot->encoded_index.end(), encoded,
                [](const auto &entry, const std::string &value) { return entry.first < value; });
            if (found != snapshot->encoded_index.end() && found->first == encoded)
                return found->second;
            return {};
        }
    }
    for (std::size_t index = 0; index < typed->size(); ++index)
        if (*typed->key_at(index) == key)
            return index;
    return {};
}
} // namespace detail
} // namespace ui
