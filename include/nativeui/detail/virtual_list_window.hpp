#pragma once

#include <nativeui/detail/virtual_list_model.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ui::detail {

template <class Key, class Payload>
class VirtualListMaterializationWindow {
public:
    using Model = VirtualListDatasetModel<Key>;
    using Item = typename Model::Item;
    using PayloadFactory = std::function<Payload(const Item&)>;

    struct MaterializedItem {
        std::string key;
        std::size_t index{};
        std::shared_ptr<const Payload> payload;
    };

    VirtualListMaterializationWindow(
        Model& model,
        float row_height,
        PayloadFactory payload_factory)
        : model_(&model),
          row_height_(row_height),
          payload_factory_(std::move(payload_factory)) {}

    [[nodiscard]] bool update(
        float scroll_y,
        float viewport_height,
        std::size_t overscan = 2,
        std::optional<std::size_t> focused_index = std::nullopt,
        std::optional<std::size_t> captured_index = std::nullopt) {
        const auto range = virtual_list_materialization_range(
            model_->size(), row_height_, scroll_y, viewport_height, overscan);
        if (!range) return false;

        const auto next_indices = virtual_list_materialized_indices(
            model_->size(), *range, focused_index, captured_index);
        if (!next_indices) return false;

        std::vector<MaterializedItem> next_items;
        std::vector<std::string> next_keys;
        next_items.reserve(next_indices->size());
        next_keys.reserve(next_indices->size());

        for (const auto index : *next_indices) {
            const auto* encoded_key = model_->encoded_key_at(index);
            const auto* item = model_->item_at(index);
            if (!encoded_key || !item) return false;

            const std::string retained_key = "virtual-list:" + *encoded_key;
            const auto existing = std::find_if(
                items_.begin(), items_.end(), [&](const MaterializedItem& materialized) {
                    return materialized.key == retained_key;
                });

            std::shared_ptr<const Payload> payload;
            if (existing != items_.end()) {
                payload = existing->payload;
            } else {
                payload = std::make_shared<const Payload>(payload_factory_(*item));
            }

            next_keys.push_back(retained_key);
            next_items.push_back(MaterializedItem{retained_key, index, std::move(payload)});
        }

        indices_ = *next_indices;
        keys_ = std::move(next_keys);
        items_ = std::move(next_items);
        return true;
    }

    [[nodiscard]] const std::vector<std::size_t>& indices() const noexcept {
        return indices_;
    }

    [[nodiscard]] const std::vector<std::string>& keys() const noexcept {
        return keys_;
    }

    [[nodiscard]] const std::vector<MaterializedItem>& items() const noexcept {
        return items_;
    }

private:
    Model* model_{};
    float row_height_{};
    PayloadFactory payload_factory_;
    std::vector<std::size_t> indices_;
    std::vector<std::string> keys_;
    std::vector<MaterializedItem> items_;
};

} // namespace ui::detail
