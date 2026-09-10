#pragma once

#include <nativeui/detail/dynamic_key.hpp>
#include <nativeui/semantics.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ui::detail {

struct VirtualListRange {
    std::size_t first{};
    std::size_t last{}; // exclusive

    [[nodiscard]] std::size_t size() const noexcept { return last - first; }
    [[nodiscard]] bool empty() const noexcept { return first == last; }

    bool operator==(const VirtualListRange&) const = default;
};

[[nodiscard]] inline std::optional<float> virtual_list_content_height(
    std::size_t item_count,
    float row_height) noexcept {
    if (!std::isfinite(row_height) || !(row_height > 0.0f)) return std::nullopt;
    if (item_count == 0) return 0.0f;

    const double extent = static_cast<double>(item_count) * static_cast<double>(row_height);
    if (!std::isfinite(extent) ||
        extent > static_cast<double>(std::numeric_limits<float>::max())) {
        return std::nullopt;
    }
    return static_cast<float>(extent);
}

[[nodiscard]] inline std::optional<VirtualListRange> virtual_list_materialization_range(
    std::size_t item_count,
    float row_height,
    float scroll_y,
    float viewport_height,
    std::size_t overscan = 2) noexcept {
    const auto content_height = virtual_list_content_height(item_count, row_height);
    if (!content_height || !std::isfinite(scroll_y) || !std::isfinite(viewport_height) ||
        scroll_y < 0.0f || viewport_height < 0.0f) {
        return std::nullopt;
    }
    if (item_count == 0 || viewport_height == 0.0f) return VirtualListRange{};

    const double content = static_cast<double>(*content_height);
    const double viewport = static_cast<double>(viewport_height);
    const double maximum_offset = std::max(0.0, content - viewport);
    const double offset = std::clamp(static_cast<double>(scroll_y), 0.0, maximum_offset);
    const double row = static_cast<double>(row_height);

    std::size_t first_visible = static_cast<std::size_t>(std::floor(offset / row));
    first_visible = std::min(first_visible, item_count - 1);

    const double visible_end = std::min(content, offset + viewport);
    std::size_t last_visible = static_cast<std::size_t>(std::ceil(visible_end / row));
    last_visible = std::clamp(last_visible, first_visible + 1, item_count);

    const std::size_t before = std::min(overscan, first_visible);
    const std::size_t after = std::min(overscan, item_count - last_visible);
    return VirtualListRange{first_visible - before, last_visible + after};
}

[[nodiscard]] inline std::optional<std::vector<std::size_t>> virtual_list_materialized_indices(
    std::size_t item_count,
    VirtualListRange range,
    std::optional<std::size_t> focused_index,
    std::optional<std::size_t> captured_index) {
    if (range.first > range.last || range.last > item_count) return std::nullopt;

    std::vector<std::size_t> result;
    result.reserve(range.size() + 2);
    for (std::size_t index = range.first; index < range.last; ++index) {
        result.push_back(index);
    }

    const auto add_exception = [&](std::optional<std::size_t> index) {
        if (!index || *index >= item_count) return;
        if (std::find(result.begin(), result.end(), *index) == result.end()) {
            result.push_back(*index);
        }
    };
    add_exception(focused_index);
    add_exception(captured_index);
    std::sort(result.begin(), result.end());
    return result;
}

template <class Key>
class VirtualListDatasetModel {
public:
    struct Item {
        Item(Key key_value,
             std::string name_value,
             bool enabled_value = true,
             std::string description_value = {},
             bool read_only_value = false,
             SemanticCheckedState checked_value = SemanticCheckedState::NotApplicable,
             std::vector<SemanticAction> actions_value = {})
            : key(std::move(key_value)),
              name(std::move(name_value)),
              enabled(enabled_value),
              description(std::move(description_value)),
              read_only(read_only_value),
              checked(checked_value),
              actions(std::move(actions_value)) {}

        Key key;
        std::string name;
        bool enabled{true};
        std::string description;
        bool read_only{};
        SemanticCheckedState checked{SemanticCheckedState::NotApplicable};
        std::vector<SemanticAction> actions;

        bool operator==(const Item&) const = default;
    };

    [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

    [[nodiscard]] const VirtualSemanticChildren::MetadataSnapshot& metadata_snapshot() const noexcept {
        return metadata_;
    }

    [[nodiscard]] const Item* item_at(std::size_t index) const noexcept {
        return index < entries_.size() ? &entries_[index].item : nullptr;
    }

    [[nodiscard]] const std::string* encoded_key_at(std::size_t index) const noexcept {
        return index < entries_.size() ? &entries_[index].encoded_key : nullptr;
    }

    [[nodiscard]] std::optional<std::size_t> index_of_key(const Key& key) const {
        const auto encoded = encode_dynamic_key(key);
        const auto it = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& entry) {
            return entry.encoded_key == encoded;
        });
        if (it == entries_.end()) return std::nullopt;
        return static_cast<std::size_t>(std::distance(entries_.begin(), it));
    }

    [[nodiscard]] std::optional<VirtualSemanticItemToken> token_for_key(const Key& key) const {
        const auto index = index_of_key(key);
        return index ? std::optional<VirtualSemanticItemToken>{entries_[*index].token} : std::nullopt;
    }

    [[nodiscard]] bool replace(std::vector<Item> items) {
        std::vector<std::string> encoded;
        encoded.reserve(items.size());
        std::unordered_set<std::string> unique;
        unique.reserve(items.size());
        for (const auto& item : items) {
            auto key = encode_dynamic_key(item.key);
            if (!unique.emplace(key).second) return false;
            encoded.push_back(std::move(key));
        }

        if (same_dataset(items, encoded)) return true;
        if (generation_ == std::numeric_limits<std::uint64_t>::max()) return false;

        std::unordered_map<std::string, VirtualSemanticItemToken> old_tokens;
        old_tokens.reserve(entries_.size());
        for (const auto& entry : entries_) old_tokens.emplace(entry.encoded_key, entry.token);

        auto next_token = next_token_;
        std::vector<Entry> next_entries;
        next_entries.reserve(items.size());
        VirtualSemanticChildren::Metadata next_metadata;
        next_metadata.reserve(items.size());

        for (std::size_t index = 0; index < items.size(); ++index) {
            VirtualSemanticItemToken token{kInvalidVirtualSemanticItemToken};
            if (const auto found = old_tokens.find(encoded[index]); found != old_tokens.end()) {
                token = found->second;
            } else {
                if (next_token == kInvalidVirtualSemanticItemToken) return false;
                token = next_token++;
            }

            VirtualSemanticItemMetadata semantic;
            semantic.token = token;
            semantic.name = items[index].name;
            semantic.description = items[index].description;
            semantic.enabled = items[index].enabled;
            semantic.read_only = items[index].read_only;
            semantic.checked = items[index].checked;
            semantic.actions = items[index].actions;
            next_metadata.push_back(std::move(semantic));
            next_entries.push_back(Entry{std::move(items[index]), std::move(encoded[index]), token});
        }

        auto next_metadata_snapshot =
            std::make_shared<const VirtualSemanticChildren::Metadata>(std::move(next_metadata));
        entries_ = std::move(next_entries);
        metadata_ = std::move(next_metadata_snapshot);
        next_token_ = next_token;
        ++generation_;
        return true;
    }

    [[nodiscard]] VirtualSemanticChildren semantic_children(
        std::optional<Key> selected,
        Rect list_bounds,
        float row_height,
        float scroll_y) const {
        std::optional<VirtualSemanticItemToken> selected_token;
        if (selected) selected_token = token_for_key(*selected);
        return VirtualSemanticChildren::from_metadata(
            generation_, metadata_, selected_token, list_bounds, row_height, scroll_y);
    }

    [[nodiscard]] VirtualSemanticChildren semantic_children(
        const Key& selected,
        Rect list_bounds,
        float row_height,
        float scroll_y) const {
        return semantic_children(std::optional<Key>{selected}, list_bounds, row_height, scroll_y);
    }

private:
    struct Entry {
        Item item;
        std::string encoded_key;
        VirtualSemanticItemToken token{kInvalidVirtualSemanticItemToken};
    };

    [[nodiscard]] bool same_dataset(const std::vector<Item>& items,
                                    const std::vector<std::string>& encoded) const {
        if (entries_.size() != items.size()) return false;
        for (std::size_t index = 0; index < items.size(); ++index) {
            if (entries_[index].encoded_key != encoded[index] || !(entries_[index].item == items[index])) {
                return false;
            }
        }
        return true;
    }

    std::vector<Entry> entries_;
    VirtualSemanticChildren::MetadataSnapshot metadata_{
        std::make_shared<const VirtualSemanticChildren::Metadata>()};
    VirtualSemanticItemToken next_token_{1};
    std::uint64_t generation_{};
};

} // namespace ui::detail
