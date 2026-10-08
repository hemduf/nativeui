#include <nativeui/collection_model.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ui::detail {
std::vector<std::size_t>
unique_collection_indices(std::size_t count,
                          const std::function<bool(std::size_t, std::size_t)> &equal) {
    if (!equal)
        throw CollectionModelError("Collection equality is missing");
    std::vector<std::size_t> result;
    result.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        bool duplicate = false;
        for (const auto previous : result)
            if (equal(previous, index)) {
                duplicate = true;
                break;
            }
        if (!duplicate)
            result.push_back(index);
    }
    return result;
}
std::shared_ptr<CollectionWriteReceipt> make_collection_write_receipt() {
    return std::make_shared<CollectionWriteReceipt>();
}
CollectionTokens::CollectionTokens()
    : snapshot_(std::make_shared<const CollectionTokenSnapshot>()) {}
PreparedCollectionTokens
CollectionTokens::prepare(std::shared_ptr<const CollectionKeys> keys) const {
    if (!keys)
        throw CollectionModelError("Collection keys are missing");
    // No borrowed registry storage crosses arbitrary Key equality/conversion.
    // A nested publication makes commit reject this stale preparation.
    const auto previous = snapshot_;
    const auto expected = previous->generation;
    auto next_token = next_token_;
    if (expected == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Collection generation exhausted");
    auto next = std::make_shared<CollectionTokenSnapshot>();
    next->generation = expected + 1;
    next->keys = std::move(keys);
    const auto count = next->keys->size();
    next->tokens.reserve(count);
    next->encoded_index.reserve(count);
    bool encoded = true;
    for (std::size_t index = 0; index < count; ++index) {
        auto value = next->keys->encoded_at(index);
        if (!value) {
            encoded = false;
            break;
        }
        next->encoded_index.emplace_back(std::move(*value), index);
    }
    if (encoded) {
        std::sort(next->encoded_index.begin(), next->encoded_index.end(),
                  [](const auto &first, const auto &second) { return first.first < second.first; });
        for (std::size_t index = 1; index < next->encoded_index.size(); ++index)
            if (next->encoded_index[index - 1].first == next->encoded_index[index].first)
                throw CollectionModelError("Collection keys must be unique");
    } else {
        next->encoded_index.clear();
        for (std::size_t index = 0; index < count; ++index)
            for (std::size_t previous_index = 0; previous_index < index; ++previous_index)
                if (next->keys->equal_at(index, *next->keys, previous_index))
                    throw CollectionModelError("Collection keys must be unique");
    }
    next->encoded = encoded;
    std::vector<std::optional<std::string>> index_keys;
    if (encoded && previous->encoded) {
        index_keys.resize(count);
        for (const auto &entry : next->encoded_index)
            index_keys[entry.second] = entry.first;
    }
    for (std::size_t index = 0; index < count; ++index) {
        std::optional<std::size_t> retained;
        if (encoded && previous->encoded) {
            const auto &value = *index_keys[index];
            const auto found = std::lower_bound(
                previous->encoded_index.begin(), previous->encoded_index.end(), value,
                [](const auto &entry, const std::string &key) { return entry.first < key; });
            if (found != previous->encoded_index.end() && found->first == value)
                retained = found->second;
        } else if (previous->keys) {
            for (std::size_t previous_index = 0; previous_index < previous->tokens.size();
                 ++previous_index)
                if (next->keys->equal_at(index, *previous->keys, previous_index)) {
                    retained = previous_index;
                    break;
                }
        }
        if (retained)
            next->tokens.push_back(previous->tokens[*retained]);
        else {
            if (next_token == kInvalidCollectionToken)
                throw std::overflow_error("Collection token space exhausted");
            next->tokens.push_back(next_token);
            next_token = next_token == std::numeric_limits<CollectionToken>::max()
                             ? kInvalidCollectionToken
                             : next_token + 1;
        }
    }
    next->token_index.reserve(count);
    for (std::size_t index = 0; index < count; ++index)
        next->token_index.emplace_back(next->tokens[index], index);
    std::sort(next->token_index.begin(), next->token_index.end(),
              [](const auto &first, const auto &second) { return first.first < second.first; });
    return {expected, next_token, std::move(next)};
}
bool CollectionTokens::commit(PreparedCollectionTokens prepared) noexcept {
    if (!prepared.snapshot || prepared.expected_generation != snapshot_->generation)
        return false;
    const auto previous = snapshot_;
    next_token_ = prepared.next_token;
    snapshot_.swap(prepared.snapshot);
    return true;
}
std::optional<std::size_t>
CollectionTokens::index_for_token(const CollectionTokenSnapshot &snapshot,
                                  CollectionToken token) noexcept {
    if (token == kInvalidCollectionToken)
        return {};
    const auto found = std::lower_bound(
        snapshot.token_index.begin(), snapshot.token_index.end(), token,
        [](const auto &entry, CollectionToken value) { return entry.first < value; });
    return found != snapshot.token_index.end() && found->first == token
               ? std::optional<std::size_t>{found->second}
               : std::nullopt;
}

struct CollectionHeightNode {
    double sum{};
    std::optional<double> uniform;
    std::shared_ptr<const CollectionHeightNode> left, right;
};
namespace {
std::shared_ptr<const CollectionHeightNode>
prepare_height_node(const std::vector<double> &sums,
                    const std::vector<std::optional<double>> &uniform, std::size_t node,
                    std::size_t span) {
    if (uniform[node])
        return std::make_shared<const CollectionHeightNode>(
            CollectionHeightNode{sums[node], uniform[node], {}, {}});
    const auto left = prepare_height_node(sums, uniform, node * 2, span / 2);
    const auto right = prepare_height_node(sums, uniform, node * 2 + 1, span / 2);
    return std::make_shared<const CollectionHeightNode>(
        CollectionHeightNode{sums[node], {}, left, right});
}
std::shared_ptr<const CollectionHeightNode>
corrected_height_node(const std::shared_ptr<const CollectionHeightNode> &old, std::size_t span,
                      std::size_t index, double value) {
    if (span == 1)
        return std::make_shared<const CollectionHeightNode>(
            CollectionHeightNode{value, value, {}, {}});
    const auto half = span / 2;
    auto left = old->left, right = old->right;
    if (old->uniform) {
        const auto sum = *old->uniform * static_cast<double>(half);
        left = std::make_shared<const CollectionHeightNode>(
            CollectionHeightNode{sum, old->uniform, {}, {}});
        right = left;
    }
    if (index < half)
        left = corrected_height_node(left, half, index, value);
    else
        right = corrected_height_node(right, half, index - half, value);
    const auto sum = left->sum + right->sum;
    if (!std::isfinite(sum))
        throw std::overflow_error("Collection content extent is not representable");
    if (left->uniform && right->uniform && *left->uniform == *right->uniform)
        return std::make_shared<const CollectionHeightNode>(
            CollectionHeightNode{sum, left->uniform, {}, {}});
    return std::make_shared<const CollectionHeightNode>(
        CollectionHeightNode{sum, {}, std::move(left), std::move(right)});
}
} // namespace
double CollectionHeightSnapshot::height_at(std::size_t index) const noexcept {
    if (index >= count_ || !root_)
        return 0.0;
    const auto *node = root_.get();
    auto span = leaf_base_;
    while (!node->uniform && span > 1) {
        const auto half = span / 2;
        if (index < half)
            node = node->left.get();
        else {
            index -= half;
            node = node->right.get();
        }
        span = half;
    }
    return node->uniform.value_or(node->sum);
}
double CollectionHeightSnapshot::prefix_sum(std::size_t end) const noexcept {
    if (end >= count_)
        return total_;
    if (!root_ || end == 0)
        return 0.0;
    const auto *node = root_.get();
    auto span = leaf_base_;
    double result = 0.0;
    while (!node->uniform && span > 1) {
        const auto half = span / 2;
        if (end <= half)
            node = node->left.get();
        else {
            result += node->left->sum;
            end -= half;
            node = node->right.get();
        }
        span = half;
    }
    return result + node->uniform.value_or(node->sum) * static_cast<double>(end);
}
std::optional<std::size_t> CollectionHeightSnapshot::index_at(double offset) const noexcept {
    if (!count_ || !root_)
        return {};
    if (std::isnan(offset) || offset < 0.0)
        offset = 0.0;
    if (offset >= total_)
        return count_ - 1;
    const auto *node = root_.get();
    auto span = leaf_base_;
    std::size_t first = 0;
    while (!node->uniform && span > 1) {
        const auto half = span / 2;
        if (offset < node->left->sum)
            node = node->left.get();
        else {
            offset -= node->left->sum;
            first += half;
            node = node->right.get();
        }
        span = half;
    }
    if (node->uniform && *node->uniform > 0.0)
        first += std::min(span - 1, static_cast<std::size_t>(offset / *node->uniform));
    return std::min(first, count_ - 1);
}
CollectionRange CollectionHeightSnapshot::range(double offset, double viewport,
                                                std::size_t overscan) const noexcept {
    if (!count_ || !std::isfinite(offset) || !std::isfinite(viewport) || !(viewport > 0.0))
        return {};
    offset = std::clamp(offset, 0.0, std::max(0.0, total_ - viewport));
    const auto first = *index_at(offset);
    const auto limit = std::min(total_, offset + viewport);
    auto last = *index_at(limit);
    if (prefix_sum(last) < limit || last == first)
        ++last;
    last = std::min(last, count_);
    return {first - std::min(first, overscan), last + std::min(count_ - last, overscan)};
}
void CollectionHeightIndex::replace(std::vector<double> heights) {
    std::size_t base = 1;
    while (base < heights.size()) {
        if (base > std::numeric_limits<std::size_t>::max() / 2)
            throw std::length_error("Collection height index is too large");
        base *= 2;
    }
    if (base > std::vector<double>{}.max_size() / 2)
        throw std::length_error("Collection height index is too large");
    std::vector<double> prepared(base * 2, 0.0);
    std::vector<std::optional<double>> uniform(base * 2, 0.0);
    for (std::size_t index = 0; index < heights.size(); ++index) {
        const auto height = heights[index];
        if (!std::isfinite(height) || !(height > 0.0))
            throw CollectionModelError("Collection row heights must be finite and positive");
        prepared[base + index] = height;
        uniform[base + index] = height;
    }
    for (auto node = base; node > 1;) {
        --node;
        prepared[node] = prepared[node * 2] + prepared[node * 2 + 1];
        if (!std::isfinite(prepared[node]))
            throw std::overflow_error("Collection content extent is not representable");
        if (uniform[node * 2] && uniform[node * 2 + 1] &&
            *uniform[node * 2] == *uniform[node * 2 + 1])
            uniform[node] = uniform[node * 2];
        else
            uniform[node].reset();
    }
    const auto total = prepared[1];
    const auto root = prepare_height_node(prepared, uniform, 1, base);
    heights_.swap(heights);
    prefix_.swap(prepared);
    root_ = root;
    total_ = total;
    leaf_base_ = base;
}
bool CollectionHeightIndex::set_height(std::size_t index, double height) {
    if (index >= heights_.size() || !std::isfinite(height) || !(height > 0.0) ||
        heights_[index] == height)
        return false;
    std::vector<std::pair<std::size_t, double>> changes;
    auto node = leaf_base_ + index;
    auto value = height;
    changes.emplace_back(node, value);
    while (node > 1) {
        const auto sibling = node % 2 == 0 ? node + 1 : node - 1;
        value += prefix_[sibling];
        if (!std::isfinite(value))
            return false;
        node /= 2;
        changes.emplace_back(node, value);
    }
    auto root = corrected_height_node(root_, leaf_base_, index, height);
    heights_[index] = height;
    for (const auto &change : changes)
        prefix_[change.first] = change.second;
    root_.swap(root);
    total_ = value;
    return true;
}
double CollectionHeightIndex::height_at(std::size_t index) const noexcept {
    return index < heights_.size() ? heights_[index] : 0.0;
}
double CollectionHeightIndex::prefix_sum(std::size_t end) const noexcept {
    return snapshot().prefix_sum(end);
}
std::optional<std::size_t> CollectionHeightIndex::index_at(double offset) const noexcept {
    return snapshot().index_at(offset);
}
CollectionRange CollectionHeightIndex::range(double offset, double viewport,
                                             std::size_t overscan) const noexcept {
    return snapshot().range(offset, viewport, overscan);
}

namespace {
std::optional<std::size_t> valid_utf8_scalar_count(std::string_view text) noexcept {
    std::size_t count = 0;
    for (std::size_t offset = 0; offset < text.size();) {
        const auto first = static_cast<unsigned char>(text[offset]);
        std::size_t length = 1;
        std::uint32_t scalar = first;
        if (first >= 0xc2 && first <= 0xdf) {
            length = 2;
            scalar = first & 0x1fU;
        } else if (first >= 0xe0 && first <= 0xef) {
            length = 3;
            scalar = first & 0x0fU;
        } else if (first >= 0xf0 && first <= 0xf4) {
            length = 4;
            scalar = first & 0x07U;
        } else if (first >= 0x80)
            return {};
        if (length > text.size() - offset)
            return {};
        for (std::size_t index = 1; index < length; ++index) {
            const auto byte = static_cast<unsigned char>(text[offset + index]);
            if ((byte & 0xc0U) != 0x80U)
                return {};
            scalar = (scalar << 6U) | (byte & 0x3fU);
        }
        if ((length == 2 && scalar < 0x80U) || (length == 3 && scalar < 0x800U) ||
            (length == 4 && scalar < 0x10000U) || scalar > 0x10ffffU ||
            (scalar >= 0xd800U && scalar <= 0xdfffU))
            return {};
        offset += length;
        ++count;
    }
    return count;
}
unsigned char ascii_fold(unsigned char value) noexcept {
    return value >= 'A' && value <= 'Z' ? static_cast<unsigned char>(value + ('a' - 'A')) : value;
}
bool ascii_equal(std::string_view first, std::string_view second) noexcept {
    if (first.size() != second.size())
        return false;
    for (std::size_t index = 0; index < first.size(); ++index)
        if (ascii_fold(static_cast<unsigned char>(first[index])) !=
            ascii_fold(static_cast<unsigned char>(second[index])))
            return false;
    return true;
}
bool label_starts_with(std::string_view label, std::string_view prefix) noexcept {
    return label.size() >= prefix.size() && ascii_equal(label.substr(0, prefix.size()), prefix);
}
} // namespace
std::optional<std::size_t> CollectionTypeahead::feed(std::string_view text, Time now,
                                                     const std::vector<CollectionSearchItem> &items,
                                                     std::optional<std::size_t> active) {
    const auto scalars = valid_utf8_scalar_count(text);
    if (!scalars || !*scalars)
        return {};
    const bool timed_out = timestamp_valid_ && last_.count() <= Time::max().count() - 700 &&
                           now.count() >= last_.count() + 700;
    const bool expired = !timestamp_valid_ || now < last_ || timed_out;
    const bool repeat = !expired && *scalars == 1 && ascii_equal(buffer_, text);
    std::string candidate = expired || repeat ? std::string{text} : buffer_ + std::string{text};
    buffer_.swap(candidate);
    last_ = now;
    timestamp_valid_ = true;
    if (items.empty())
        return {};
    const auto start =
        active && *active < items.size() ? (*active + (repeat ? 1U : 0U)) % items.size() : 0;
    for (std::size_t distance = 0; distance < items.size(); ++distance) {
        const auto index =
            distance < items.size() - start ? start + distance : distance - (items.size() - start);
        if (items[index].eligible && label_starts_with(items[index].label, buffer_))
            return index;
    }
    return {};
}
void CollectionTypeahead::clear() noexcept {
    buffer_.clear();
    last_ = Time{};
    timestamp_valid_ = false;
}

CollectionSelection resolve_collection_selection(const std::vector<CollectionToken> &order,
                                                 const std::vector<bool> &eligible,
                                                 const CollectionSelection &current, bool multiple,
                                                 std::optional<CollectionToken> target,
                                                 CollectionSelectionGesture gesture) {
    if (eligible.size() != order.size())
        throw CollectionModelError("Collection eligibility/order size mismatch");
    const auto index_for = [&](std::optional<CollectionToken> token) -> std::optional<std::size_t> {
        if (!token)
            return {};
        const auto found = std::find(order.begin(), order.end(), *token);
        if (found == order.end())
            return {};
        const auto index = static_cast<std::size_t>(std::distance(order.begin(), found));
        return eligible[index] ? std::optional<std::size_t>{index} : std::nullopt;
    };
    auto selected = current.selected;
    std::sort(selected.begin(), selected.end());
    CollectionSelection result;
    for (std::size_t index = 0; index < order.size(); ++index) {
        if (eligible[index] && std::binary_search(selected.begin(), selected.end(), order[index])) {
            result.selected.push_back(order[index]);
            if (!multiple)
                break;
        }
    }
    const auto old_active = index_for(current.active), old_anchor = index_for(current.anchor);
    if (old_active)
        result.active = order[*old_active];
    if (old_anchor)
        result.anchor = order[*old_anchor];
    if (gesture == CollectionSelectionGesture::SelectAll) {
        if (!multiple)
            return result;
        result.selected.clear();
        for (std::size_t index = 0; index < order.size(); ++index)
            if (eligible[index])
                result.selected.push_back(order[index]);
        if (!result.active && !result.selected.empty())
            result.active = result.selected.front();
        if (!result.anchor)
            result.anchor = result.active;
        return result;
    }
    const auto target_index = index_for(target);
    if (!target_index)
        return result;
    result.active = order[*target_index];
    if (gesture == CollectionSelectionGesture::MoveActive)
        return result;
    if (!multiple || gesture == CollectionSelectionGesture::Replace) {
        result.selected = {order[*target_index]};
        result.anchor = result.active;
        return result;
    }
    if (gesture == CollectionSelectionGesture::Toggle) {
        const auto found =
            std::find(result.selected.begin(), result.selected.end(), *result.active);
        if (found == result.selected.end())
            result.selected.push_back(*result.active);
        else
            result.selected.erase(found);
        result.anchor = result.active;
    } else {
        const auto origin = old_anchor.value_or(old_active.value_or(*target_index));
        result.anchor = order[origin];
        if (gesture == CollectionSelectionGesture::Extend)
            result.selected.clear();
        for (std::size_t index = std::min(origin, *target_index);
             index <= std::max(origin, *target_index); ++index)
            if (eligible[index] && std::find(result.selected.begin(), result.selected.end(),
                                             order[index]) == result.selected.end())
                result.selected.push_back(order[index]);
    }
    selected = result.selected;
    std::sort(selected.begin(), selected.end());
    std::vector<CollectionToken> canonical;
    canonical.reserve(result.selected.size());
    for (std::size_t index = 0; index < order.size(); ++index)
        if (eligible[index] && std::binary_search(selected.begin(), selected.end(), order[index]))
            canonical.push_back(order[index]);
    result.selected.swap(canonical);
    return result;
}
} // namespace ui::detail
