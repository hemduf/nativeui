#pragma once

#include <nativeui/detail/collection_view_kernel.hpp>
#include <nativeui/label.hpp>

#include <algorithm>
#include <memory>
#include <type_traits>
#include <stdexcept>
#include <unordered_set>

namespace ui::detail {
template <class Key> class CollectionControllerHandle {
  public:
    CollectionControllerHandle() : controller_(std::make_shared<CollectionController>()) {}
    [[nodiscard]] const std::shared_ptr<CollectionController> &controller() const noexcept {
        return controller_;
    }
    bool scroll_to_key(const Key &key, ScrollAlignment alignment = ScrollAlignment::Nearest) {
        const auto session = controller_->primary();
        if (!session)
            return false;
        const auto tokens = session->token_snapshot();
        const auto index = collection_index_of_key(tokens, key);
        return index && session->attached() &&
               session->scroll_to(tokens->tokens[*index], alignment);
    }
    bool scroll_to_index(std::size_t index, ScrollAlignment alignment = ScrollAlignment::Nearest) {
        const auto session = controller_->primary();
        if (!session)
            return false;
        const auto visible = session->visible_tokens();
        return index < visible.size() && session->attached() &&
               session->scroll_to(visible[index], alignment);
    }
    [[nodiscard]] CollectionRange visible_range() const noexcept {
        const auto session = controller_->primary();
        return session ? session->visible_range() : CollectionRange{};
    }
    [[nodiscard]] std::vector<Key> visible_rows() const {
        const auto session = controller_->primary();
        if (!session)
            return {};
        const auto tokens = session->token_snapshot();
        const auto visible = session->visible_tokens();
        const auto *keys = dynamic_cast<const TypedCollectionKeys<Key> *>(tokens->keys.get());
        if (!keys)
            return {};
        std::vector<Key> result;
        result.reserve(visible.size());
        for (const auto token : visible)
            if (const auto index = CollectionTokens::index_for_token(*tokens, token))
                result.push_back(*keys->key_at(*index));
        return result;
    }
    [[nodiscard]] std::optional<Key> item_at_visible_index(std::size_t index) const {
        const auto session = controller_->primary();
        if (!session)
            return {};
        const auto tokens = session->token_snapshot();
        const auto visible = session->visible_tokens();
        const auto *keys = dynamic_cast<const TypedCollectionKeys<Key> *>(tokens->keys.get());
        if (!keys || index >= visible.size())
            return {};
        const auto row = CollectionTokens::index_for_token(*tokens, visible[index]);
        return row ? std::optional<Key>{*keys->key_at(*row)} : std::nullopt;
    }
    [[nodiscard]] std::optional<std::size_t> depth(const Key &key) const {
        const auto session = controller_->primary();
        if (!session)
            return {};
        const auto tokens = session->token_snapshot();
        const auto index = collection_index_of_key(tokens, key);
        return index ? session->depth_for_token(tokens->tokens[*index]) : std::nullopt;
    }
    [[nodiscard]] VirtualSemanticChildren semantic_children() const {
        const auto session = controller_->primary();
        return session ? session->semantic_children() : VirtualSemanticChildren{};
    }

  private:
    std::shared_ptr<CollectionController> controller_;
};

template <class Subscription>
class OwnedCollectionSubscription final : public CollectionSubscription {
  public:
    explicit OwnedCollectionSubscription(Subscription value) : value_(std::move(value)) {}

  private:
    Subscription value_;
};
class CollectionSubscriptionGroup final : public CollectionSubscription {
  public:
    std::vector<std::unique_ptr<CollectionSubscription>> members;
};
template <class Key, class Item> struct CollectionSourceRecipe {
    CollectionSourceRecipe(Binding<std::vector<Item>> value, Binding<SelectionSnapshot<Key>> chosen,
                           std::optional<Binding<std::vector<Key>>> opened)
        : rows(std::move(value)), selection(std::move(chosen)), expanded(std::move(opened)) {}
    Binding<std::vector<Item>> rows;
    Binding<SelectionSnapshot<Key>> selection;
    std::optional<Binding<std::vector<Key>>> expanded;
    std::function<Spec(const Item &)> row;
    std::function<Spec(const Item &, const std::string &)> cell;
    std::function<void(const Key &)> activation;
    std::function<void(const std::vector<Key> &)> expansion_change;
    std::function<void(const std::vector<Key> &, std::optional<Key>)> reorder;
    bool table{};
};
[[nodiscard]] Spec make_collection_cell_strip(std::vector<Spec> cells,
                                              std::vector<std::string> columns);

template <class Key, class Item> class TypedCollectionSource final : public CollectionSource {
  public:
    explicit TypedCollectionSource(std::shared_ptr<const CollectionSourceRecipe<Key, Item>> recipe)
        : recipe_(std::move(recipe)) {}
    [[nodiscard]] CollectionSourceVersion version() const noexcept override {
        return {recipe_->rows.revision(), recipe_->expanded ? recipe_->expanded->revision() : 0};
    }
    [[nodiscard]] std::uint64_t selection_revision() const noexcept override {
        return recipe_->selection.revision();
    }
    [[nodiscard]] bool valid() const noexcept override { return recipe_->rows.valid(); }
    [[nodiscard]] bool selection_valid() const noexcept override {
        return recipe_->selection.valid();
    }
    [[nodiscard]] bool expansion_valid() const noexcept override {
        return !recipe_->expanded || recipe_->expanded->valid();
    }
    [[nodiscard]] std::optional<CollectionInput> read() const override {
        const auto recipe = recipe_;
        const auto before = version();
        auto rows = std::make_shared<const std::vector<Item>>(recipe->rows.snapshot());
        if (version() != before)
            return {};
        std::shared_ptr<const std::vector<Key>> expansion;
        if (recipe->expanded)
            expansion = std::make_shared<const std::vector<Key>>(recipe->expanded->snapshot());
        if (version() != before)
            return {};
        std::vector<Key> keys;
        keys.reserve(rows->size());
        CollectionInput result;
        result.version = before;
        result.rows.reserve(rows->size());
        result.sections.reserve(rows->size());
        result.branches.reserve(rows->size());
        for (const auto &item : *rows) {
            keys.push_back(item.key);
            if constexpr (requires { item.section_header; }) {
                result.rows.push_back({item.label, item.enabled && !item.section_header});
                result.sections.push_back(item.section_header);
                result.branches.push_back(false);
            } else {
                result.rows.push_back({item.label, item.enabled});
                result.sections.push_back(false);
                result.branches.push_back(item.branch);
            }
        }
        result.keys = std::make_shared<const TypedCollectionKeys<Key>>(std::move(keys));
        if constexpr (requires(const Item &item) { item.parent; }) {
            result.parents = [rows](std::shared_ptr<const CollectionTokenSnapshot> tokens) {
                std::vector<std::optional<std::size_t>> parents;
                parents.reserve(rows->size());
                for (const auto &item : *rows) {
                    if (!item.parent)
                        parents.push_back({});
                    else {
                        const auto index = collection_index_of_key(tokens, *item.parent);
                        if (!index)
                            throw CollectionModelError("collection parent key is missing");
                        parents.push_back(index);
                    }
                }
                return parents;
            };
        }
        result.expanded = [expansion](std::shared_ptr<const CollectionTokenSnapshot> tokens) {
            std::vector<CollectionToken> result;
            if (expansion)
                for (const auto &key : *expansion)
                    if (const auto index = collection_index_of_key(tokens, key))
                        result.push_back(tokens->tokens[*index]);
            return result;
        };
        result.make_row = [recipe, rows](std::size_t index,
                                         const std::vector<std::string> &columns) {
            if (index >= rows->size())
                throw std::out_of_range("collection row index");
            const auto &item = (*rows)[index];
            if (recipe->table) {
                std::vector<Spec> cells;
                cells.reserve(columns.size());
                for (const auto &column : columns) {
                    if (recipe->cell)
                        cells.push_back(recipe->cell(item, column));
                    else
                        cells.push_back(std::move(Label{item.label}).spec());
                    if constexpr (requires { item.section_header; })
                        if (item.section_header)
                            break;
                }
                return make_collection_cell_strip(std::move(cells), columns);
            }
            if (recipe->row)
                return recipe->row(item);
            return std::move(Label{item.label}).spec();
        };
        if (version() != before)
            return {};
        return result;
    }
    [[nodiscard]] std::optional<CollectionExpansionRead>
    read_expansion(std::shared_ptr<const CollectionTokenSnapshot> tokens) const override {
        const auto recipe = recipe_;
        const auto before = version();
        std::vector<CollectionToken> result;
        if (recipe->expanded) {
            const auto keys = recipe->expanded->snapshot();
            if (version() != before)
                return {};
            result.reserve(keys.size());
            for (const auto &key : keys)
                if (const auto i = collection_index_of_key(tokens, key))
                    result.push_back(tokens->tokens[*i]);
        }
        if (version() != before)
            return {};
        return CollectionExpansionRead{before, std::move(result)};
    }
    [[nodiscard]] std::optional<CollectionSelectionRead>
    read_selection(std::shared_ptr<const CollectionTokenSnapshot> tokens) const override {
        const auto recipe = recipe_;
        const auto before = selection_revision();
        const auto rows_version = version();
        const auto source = recipe->selection.snapshot();
        if (selection_revision() != before || version() != rows_version)
            return {};
        CollectionSelection selection;
        for (const auto &key : source.selected)
            if (const auto i = collection_index_of_key(tokens, key))
                selection.selected.push_back(tokens->tokens[*i]);
        if (source.active)
            if (const auto i = collection_index_of_key(tokens, *source.active))
                selection.active = tokens->tokens[*i];
        if (source.anchor)
            if (const auto i = collection_index_of_key(tokens, *source.anchor))
                selection.anchor = tokens->tokens[*i];
        std::unordered_set<CollectionToken> seen;
        selection.selected.erase(
            std::remove_if(selection.selected.begin(), selection.selected.end(),
                           [&](CollectionToken token) { return !seen.insert(token).second; }),
            selection.selected.end());
        if (selection_revision() != before || version() != rows_version)
            return {};
        return CollectionSelectionRead{before, std::move(selection)};
    }
    bool publish_selection(std::shared_ptr<const CollectionTokenSnapshot> tokens,
                           CollectionSelection candidate,
                           const std::function<bool()> &permitted) override {
        const auto recipe = recipe_;
        const auto revision = recipe->selection.revision();
        const auto rows_version = version();
        if (!valid() || !selection_valid() || !permitted())
            return false;
        const auto *keys = dynamic_cast<const TypedCollectionKeys<Key> *>(tokens->keys.get());
        if (!keys)
            return false;
        SelectionSnapshot<Key> output;
        for (const auto token : candidate.selected)
            if (const auto index = CollectionTokens::index_for_token(*tokens, token))
                output.selected.push_back(*keys->key_at(*index));
        if (candidate.active)
            if (const auto index = CollectionTokens::index_for_token(*tokens, *candidate.active))
                output.active = *keys->key_at(*index);
        if (candidate.anchor)
            if (const auto index = CollectionTokens::index_for_token(*tokens, *candidate.anchor))
                output.anchor = *keys->key_at(*index);
        if (!valid() || !selection_valid() || version() != rows_version ||
            selection_revision() != revision || !permitted())
            return false;
        const auto receipt = make_collection_write_receipt();
        auto binding = recipe->selection;
        binding.set_if(std::move(output), [recipe, rows_version, revision, permitted, receipt] {
            if (!recipe->rows.valid() || !recipe->selection.valid() ||
                recipe->rows.revision() != rows_version.rows ||
                (recipe->expanded && recipe->expanded->revision() != rows_version.expansion) ||
                recipe->selection.revision() != revision || !permitted())
                return false;
            receipt->accepted = true;
            return true;
        });
        return receipt->accepted;
    }
    bool publish_expanded(std::shared_ptr<const CollectionTokenSnapshot> tokens,
                          std::vector<CollectionToken> candidate,
                          const std::function<bool()> &permitted) override {
        const auto recipe = recipe_;
        if (!recipe->expanded || !recipe->expanded->valid() || !valid() || !permitted())
            return false;
        const auto before = version();
        const auto *keys = dynamic_cast<const TypedCollectionKeys<Key> *>(tokens->keys.get());
        if (!keys)
            return false;
        std::vector<Key> output;
        output.reserve(candidate.size());
        for (const auto token : candidate)
            if (const auto index = CollectionTokens::index_for_token(*tokens, token))
                output.push_back(*keys->key_at(*index));
        if (version() != before || !valid() || !permitted())
            return false;
        const auto receipt = make_collection_write_receipt();
        auto binding = *recipe->expanded;
        binding.set_if(output, [recipe, before, permitted, receipt] {
            if (!recipe->rows.valid() || !recipe->expanded->valid() ||
                recipe->rows.revision() != before.rows ||
                recipe->expanded->revision() != before.expansion || !permitted())
                return false;
            receipt->accepted = true;
            return true;
        });
        if (!receipt->accepted || !valid() || !recipe->expanded->valid() ||
            recipe->rows.revision() != before.rows ||
            recipe->expanded->revision() != before.expansion + 1 || !permitted())
            return receipt->accepted;
        const auto callback = recipe->expansion_change;
        if (valid() && recipe->expanded->valid() && recipe->rows.revision() == before.rows &&
            recipe->expanded->revision() == before.expansion + 1 && permitted() && callback)
            callback(output);
        return receipt->accepted;
    }
    void activate(std::shared_ptr<const CollectionTokenSnapshot> tokens, CollectionToken token,
                  const std::function<bool()> &permitted) override {
        const auto recipe = recipe_;
        const auto before = version();
        if (!valid() || !permitted())
            return;
        const auto index = CollectionTokens::index_for_token(*tokens, token);
        const auto *keys = dynamic_cast<const TypedCollectionKeys<Key> *>(tokens->keys.get());
        if (!index || !keys)
            return;
        const Key key = *keys->key_at(*index);
        const auto callback = recipe->activation;
        if (valid() && version() == before && permitted() && callback)
            callback(key);
    }
    void reorder(std::shared_ptr<const CollectionTokenSnapshot> tokens,
                 std::vector<CollectionToken> selected, std::optional<CollectionToken> before_token,
                 const std::function<bool()> &permitted) override {
        const auto recipe = recipe_;
        const auto before = version();
        const auto selected_revision = selection_revision();
        if (!valid() || !permitted())
            return;
        const auto *keys = dynamic_cast<const TypedCollectionKeys<Key> *>(tokens->keys.get());
        if (!keys)
            return;
        std::vector<Key> output;
        output.reserve(selected.size());
        for (const auto token : selected)
            if (const auto index = CollectionTokens::index_for_token(*tokens, token))
                output.push_back(*keys->key_at(*index));
        std::optional<Key> insertion;
        if (before_token)
            if (const auto index = CollectionTokens::index_for_token(*tokens, *before_token))
                insertion = *keys->key_at(*index);
        const auto callback = recipe->reorder;
        if (valid() && version() == before && selection_revision() == selected_revision &&
            permitted() && callback)
            callback(output, std::move(insertion));
    }
    std::unique_ptr<CollectionSubscription> observe(std::function<void()> changed) override {
        auto group = std::make_unique<CollectionSubscriptionGroup>();
        const auto add = [&](auto binding) {
            using BindingType = decltype(binding);
            auto subscription = binding.observe([changed](const auto &) {
                if (changed)
                    changed();
            });
            group->members.push_back(
                std::make_unique<OwnedCollectionSubscription<typename BindingType::Subscription>>(
                    std::move(subscription)));
        };
        add(recipe_->rows);
        add(recipe_->selection);
        if (recipe_->expanded)
            add(*recipe_->expanded);
        return group;
    }

  private:
    std::shared_ptr<const CollectionSourceRecipe<Key, Item>> recipe_;
};
template <class Key, class Item>
[[nodiscard]] CollectionSourceFactory
collection_source_factory(CollectionSourceRecipe<Key, Item> recipe) {
    auto owned = std::make_shared<const CollectionSourceRecipe<Key, Item>>(std::move(recipe));
    return [owned] { return std::make_shared<TypedCollectionSource<Key, Item>>(owned); };
}
} // namespace ui::detail
