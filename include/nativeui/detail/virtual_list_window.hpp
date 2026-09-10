#pragma once

#include <nativeui/component_base.hpp>
#include <nativeui/detail/dynamic_source.hpp>
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

template <class Key>
class VirtualListMaterializationWindow {
public:
    using Model = VirtualListDatasetModel<Key>;
    using Item = typename Model::Item;
    using RowFactory = std::function<Spec(const Item&)>;

    VirtualListMaterializationWindow(Model& model, float row_height, RowFactory row_factory)
        : model_(&model), row_height_(row_height), row_factory_(std::move(row_factory)) {}

    [[nodiscard]] bool update(
        float scroll_y,
        float viewport_height,
        std::size_t overscan = 2,
        std::optional<Key> focused_key = std::nullopt,
        std::optional<Key> captured_key = std::nullopt) {
        const auto next_indices = model_->materialized_indices(
            row_height_,
            scroll_y,
            viewport_height,
            overscan,
            std::move(focused_key),
            std::move(captured_key));
        if (!next_indices) return false;

        std::vector<Slot> next_slots;
        std::vector<std::string> next_keys;
        next_slots.reserve(next_indices->size());
        next_keys.reserve(next_indices->size());

        for (const auto index : *next_indices) {
            const auto* encoded_key = model_->encoded_key_at(index);
            const auto* item = model_->item_at(index);
            if (!encoded_key || !item) return false;

            const std::string retained_key = "virtual-list:" + *encoded_key;
            const auto existing = std::find_if(
                slots_.begin(), slots_.end(), [&](const Slot& slot) {
                    return slot.key == retained_key;
                });

            std::shared_ptr<const Spec> spec;
            if (existing != slots_.end()) {
                spec = existing->spec;
            } else {
                spec = std::make_shared<const Spec>(row_factory_(*item));
            }

            next_keys.push_back(retained_key);
            next_slots.push_back(Slot{retained_key, index, std::move(spec)});
        }

        indices_ = *next_indices;
        keys_ = std::move(next_keys);
        slots_ = std::move(next_slots);
        return true;
    }

    [[nodiscard]] const std::vector<std::size_t>& indices() const noexcept {
        return indices_;
    }

    [[nodiscard]] const std::vector<std::string>& keys() const noexcept {
        return keys_;
    }

    [[nodiscard]] std::vector<DynamicChildSpec> children() const {
        std::vector<DynamicChildSpec> result;
        result.reserve(slots_.size());
        for (const auto& slot : slots_) {
            result.push_back(DynamicChildSpec{slot.key, *slot.spec});
        }
        return result;
    }

private:
    struct Slot {
        std::string key;
        std::size_t index{};
        std::shared_ptr<const Spec> spec;
    };

    Model* model_{};
    float row_height_{};
    RowFactory row_factory_;
    std::vector<std::size_t> indices_;
    std::vector<std::string> keys_;
    std::vector<Slot> slots_;
};

} // namespace ui::detail
