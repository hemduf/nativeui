#pragma once

#include <nativeui/component.hpp>
#include <nativeui/list_tabs_style.hpp>
#include <nativeui/state.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace ui::detail {
class ListSubscription {
public:
    virtual ~ListSubscription() = default;
};
class ListSelection {
public:
    virtual ~ListSelection() = default;
    [[nodiscard]] virtual bool valid() const noexcept = 0;
    [[nodiscard]] virtual std::vector<bool> snapshot() const = 0;
    virtual void publish(std::size_t index,bool activate,const std::function<bool(bool)>& permitted) = 0;
    [[nodiscard]] virtual std::unique_ptr<ListSubscription> observe(std::function<void()> changed) = 0;
};
template <class Key>
class TypedListSelection final : public ListSelection {
    class Subscription final : public ListSubscription {
    public:
        explicit Subscription(typename Binding<std::optional<Key>>::Subscription value) : value_(std::move(value)) {}
    private:
        typename Binding<std::optional<Key>>::Subscription value_;
    };
public:
    TypedListSelection(Binding<std::optional<Key>> source,std::shared_ptr<const std::vector<Key>> keys,
                       std::function<void(const Key&)> activation)
        : source_(std::move(source)),keys_(std::move(keys)),
          activation_(std::make_shared<const std::function<void(const Key&)>>(std::move(activation))) {}
    [[nodiscard]] bool valid() const noexcept override { return source_.valid(); }
    [[nodiscard]] std::vector<bool> snapshot() const override {
        // Own all adapter storage before Key copies or equality can retire it.
        const auto source = source_;
        const auto keys = keys_;
        for (unsigned attempt = 0; attempt < 8; ++attempt) {
            const auto revision = source.revision();
            const auto selected = source.snapshot();
            if (source.revision() != revision) continue;
            std::vector<bool> result;
            result.reserve(keys->size());
            for (const auto& key : *keys) result.push_back(selected && key == *selected);
            if (source.revision() == revision) return result;
        }
        throw std::runtime_error("ListView selection snapshot did not stabilize");
    }
    void publish(std::size_t index,bool activate,const std::function<bool(bool)>& permitted) override {
        const auto source = source_;
        const auto keys = keys_;
        const auto activation = activation_;
        const auto revision = source.revision();
        const auto permission = permitted;
        const auto allowed = [source,permission](bool committed,std::uint64_t expected) {
            return permission(committed) && source.valid() && source.revision() == expected;
        };
        if (index >= keys->size() || !allowed(false,revision)) return;
        const auto callback = activate ? *activation : std::function<void(const Key&)>{};
        const auto key = (*keys)[index];
        std::optional<Key> candidate{key};
        if (!allowed(false,revision)) return;
        const auto accepted = std::make_shared<bool>(false);
        auto binding = source;
        binding.set_if(std::move(candidate),[allowed,revision,accepted] {
            if (!allowed(false,revision)) return false;
            *accepted = true;
            return true;
        });
        if (!callback) return;
        // An equal selection remains activatable. Any changed selection must
        // belong to this write; a reentrant replacement cannot activate it.
        const auto expected = *accepted ? revision + 1 : revision;
        if (!allowed(true,expected)) return;
        const auto authoritative = source.snapshot();
        if (!allowed(true,expected)) return;
        const bool selected = authoritative && *authoritative == key;
        if (selected && allowed(true,expected)) callback(key);
    }
    [[nodiscard]] std::unique_ptr<ListSubscription> observe(std::function<void()> changed) override {
        return std::make_unique<Subscription>(source_.observe([changed=std::move(changed)](const auto&) {
            if (changed) changed();
        }));
    }
private:
    Binding<std::optional<Key>> source_;
    std::shared_ptr<const std::vector<Key>> keys_;
    std::shared_ptr<const std::function<void(const Key&)>> activation_;
};
struct ListRecipe {
    ListViewStyle style;
    std::vector<bool> enabled;
    std::vector<Spec> rows;
};
[[nodiscard]] Spec make_list_view_spec(std::shared_ptr<const ListRecipe> recipe,
    std::function<std::unique_ptr<ListSelection>()> selection_factory);
} // namespace ui::detail
