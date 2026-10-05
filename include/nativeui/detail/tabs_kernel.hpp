#pragma once

#include <nativeui/component.hpp>
#include <nativeui/list_tabs_style.hpp>
#include <nativeui/state.hpp>

#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

namespace ui::detail {
class TabsSubscription {
public:
    virtual ~TabsSubscription() = default;
};
class TabsSelection {
public:
    virtual ~TabsSelection() = default;
    [[nodiscard]] virtual bool valid() const noexcept = 0;
    [[nodiscard]] virtual std::vector<bool> snapshot() const = 0;
    virtual void select(std::size_t index,const std::function<bool()>& permitted) = 0;
    [[nodiscard]] virtual std::unique_ptr<TabsSubscription> observe(std::function<void()> changed) = 0;
};
template <class T>
class TypedTabsSelection final : public TabsSelection {
    class Subscription final : public TabsSubscription {
    public:
        explicit Subscription(typename Binding<T>::Subscription value) : value_(std::move(value)) {}
    private:
        typename Binding<T>::Subscription value_;
    };
public:
    TypedTabsSelection(Binding<T> source,std::shared_ptr<const std::vector<T>> keys)
        : source_(std::move(source)),keys_(std::move(keys)) {}
    [[nodiscard]] bool valid() const noexcept override { return source_.valid(); }
    [[nodiscard]] std::vector<bool> snapshot() const override {
        // Key copies and equality may replace the source or retire the adapter.
        const auto source = source_;
        const auto keys = keys_;
        for (unsigned attempt = 0; attempt < 8; ++attempt) {
            const auto revision = source.revision();
            const auto selected = source.snapshot();
            if (source.revision() != revision) continue;
            std::vector<bool> result;
            result.reserve(keys->size());
            for (const auto& key : *keys) result.push_back(key == selected);
            if (source.revision() == revision) return result;
        }
        throw std::runtime_error("Tabs selection snapshot did not stabilize");
    }
    void select(std::size_t index,const std::function<bool()>& permitted) override {
        const auto source = source_;
        const auto keys = keys_;
        const auto revision = source.revision();
        const auto permission = permitted;
        const auto allowed = [source,revision,permission] {
            return permission() && source.valid() && source.revision() == revision;
        };
        if (index >= keys->size() || !allowed()) return;
        const auto key = (*keys)[index];
        if (!allowed()) return;
        auto binding = source;
        binding.set_if(key,allowed);
    }
    [[nodiscard]] std::unique_ptr<TabsSubscription> observe(std::function<void()> changed) override {
        return std::make_unique<Subscription>(source_.observe([changed=std::move(changed)](const T&) {
            if (changed) changed();
        }));
    }
private:
    Binding<T> source_;
    std::shared_ptr<const std::vector<T>> keys_;
};
struct TabsRecipe {
    TabsStyle style;
    std::vector<std::string> labels;
    std::vector<bool> enabled;
    std::vector<Spec> panels;
};
[[nodiscard]] Spec make_tabs_spec(std::shared_ptr<const TabsRecipe> recipe,
    std::function<std::unique_ptr<TabsSelection>()> selection_factory);
} // namespace ui::detail
