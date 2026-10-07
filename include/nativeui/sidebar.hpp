#pragma once

#include <nativeui/component.hpp>
#include <nativeui/state.hpp>
#include <nativeui/list_tabs_style.hpp>

#include <algorithm>
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace ui {
struct SidebarStyle {
    ListViewStyle rows;
    std::optional<Color> surface, text, section_text;
    double padding{8}, indentation{16}, gap{2}, row_height{32}, section_height{28};
    double preferred_width{220}, minimum_width{120};
    std::optional<float> text_size, section_text_size;
};
namespace detail {
struct SidebarSelectionRead {
    std::uint64_t revision{};
    std::optional<std::size_t> selected;
};
class SidebarSelectionSubscription {
  public:
    virtual ~SidebarSelectionSubscription() = default;
};
class SidebarSelection {
  public:
    virtual ~SidebarSelection() = default;
    [[nodiscard]] virtual bool valid() const noexcept = 0;
    [[nodiscard]] virtual std::uint64_t revision() const noexcept = 0;
    [[nodiscard]] virtual std::optional<SidebarSelectionRead> read() const = 0;
    virtual void choose(std::size_t, bool, const std::function<bool()> &) = 0;
    [[nodiscard]] virtual std::unique_ptr<SidebarSelectionSubscription>
        observe(std::function<void()>) = 0;
};
struct SidebarSectionRecipe {
    std::string id, title;
    std::optional<Binding<bool>> open;
};
struct SidebarRowRecipe {
    std::string label;
    std::optional<std::size_t> section, item;
    bool header{}, enabled{true};
    std::optional<Spec> accessory;
};
struct SidebarRecipe {
    std::vector<SidebarSectionRecipe> sections;
    std::vector<SidebarRowRecipe> rows;
    std::function<std::shared_ptr<SidebarSelection>()> selection;
    SidebarStyle style;
};
[[nodiscard]] Spec make_sidebar(SidebarRecipe);

template <class Key> class TypedSidebarSelection final : public SidebarSelection {
  public:
    TypedSidebarSelection(Binding<std::optional<Key>> selection,
                          std::shared_ptr<const std::vector<Key>> keys,
                          std::function<void(const Key &)> navigate)
        : selection_(std::move(selection)), keys_(std::move(keys)), navigate_(std::move(navigate)) {
    }
    [[nodiscard]] bool valid() const noexcept override { return selection_.valid(); }
    [[nodiscard]] std::uint64_t revision() const noexcept override { return selection_.revision(); }
    [[nodiscard]] std::optional<SidebarSelectionRead> read() const override {
        const auto binding = selection_;
        const auto keys = keys_;
        const auto before = binding.revision();
        const auto value = binding.snapshot();
        if (binding.revision() != before)
            return {};
        std::optional<std::size_t> selected;
        if (value)
            for (std::size_t i = 0; i < keys->size(); ++i)
                if ((*keys)[i] == *value) {
                    selected = i;
                    break;
                }
        if (binding.revision() != before)
            return {};
        return SidebarSelectionRead{before, selected};
    }
    void choose(std::size_t index, bool activate, const std::function<bool()> &permitted) override {
        auto binding = selection_;
        const auto keys = keys_;
        if (!binding.valid() || index >= keys->size() || !permitted())
            return;
        const auto before = binding.revision();
        const auto value = binding.snapshot();
        if (!binding.valid() || binding.revision() != before || !permitted())
            return;
        const Key key = (*keys)[index];
        const bool same = value && *value == key;
        if (!binding.valid() || binding.revision() != before || !permitted())
            return;
        if (!same) {
            struct Receipt {
                bool accepted{};
            };
            const auto receipt = std::make_shared<Receipt>();
            binding.set_if(std::optional<Key>{key}, [binding, before, receipt, permitted] {
                if (!binding.valid() || binding.revision() != before || !permitted())
                    return false;
                receipt->accepted = true;
                return true;
            });
            if (!receipt->accepted || !binding.valid() || binding.revision() != before + 1 ||
                !permitted())
                return;
        } else if (!activate)
            return;
        const auto callback = navigate_;
        if (binding.valid() && binding.revision() == before + (same ? 0 : 1) && permitted() &&
            callback)
            callback(key);
    }

    [[nodiscard]] std::unique_ptr<SidebarSelectionSubscription>
    observe(std::function<void()> changed) override {
        class Subscription final : public SidebarSelectionSubscription {
          public:
            explicit Subscription(typename Binding<std::optional<Key>>::Subscription value)
                : value_(std::move(value)) {}

          private:
            typename Binding<std::optional<Key>>::Subscription value_;
        };
        auto binding = selection_;
        return std::make_unique<Subscription>(
            binding.observe([changed = std::move(changed)](const auto &) {
                if (changed)
                    changed();
            }));
    }

  private:
    Binding<std::optional<Key>> selection_;
    std::shared_ptr<const std::vector<Key>> keys_;
    std::function<void(const Key &)> navigate_;
};
} // namespace detail

template <class Key> class Sidebar {
  public:
    explicit Sidebar(Binding<std::optional<Key>> selection) : selection_(std::move(selection)) {}
    explicit Sidebar(State<std::optional<Key>> &selection) : Sidebar(selection.binding()) {}
    Sidebar &&section(std::string id, std::string title, Binding<bool> open) && {
        return std::move(*this).add_section(std::move(id), std::move(title), std::move(open));
    }
    Sidebar &&section(std::string id, std::string title, State<bool> &open) && {
        return std::move(*this).section(std::move(id), std::move(title), open.binding());
    }
    Sidebar &&section(std::string id, std::string title) && {
        return std::move(*this).add_section(std::move(id), std::move(title), {});
    }
    Sidebar &&item(Key key, std::string label, bool enabled = true) && {
        add_item(std::move(key), std::move(label), {}, enabled);
        return std::move(*this);
    }
    template <class Accessory>
        requires(!std::same_as<std::remove_cvref_t<Accessory>, bool>)
    Sidebar &&item(Key key, std::string label, Accessory &&accessory, bool enabled = true) && {
        add_item(std::move(key), std::move(label), make_spec(std::forward<Accessory>(accessory)),
                 enabled);
        return std::move(*this);
    }
    Sidebar &&on_navigate(std::function<void(const Key &)> value) && {
        navigate_ = std::move(value);
        return std::move(*this);
    }
    Sidebar &&style(SidebarStyle value) && {
        recipe_.style = std::move(value);
        return std::move(*this);
    }
    [[nodiscard]] Spec spec() && {
        for (std::size_t i = 0; i < keys_.size(); ++i)
            for (std::size_t j = 0; j < i; ++j)
                if (keys_[i] == keys_[j])
                    throw std::invalid_argument("Sidebar item keys must be unique");
        auto keys = std::make_shared<const std::vector<Key>>(std::move(keys_));
        recipe_.selection = [binding = selection_, keys, navigate = std::move(navigate_)] {
            return std::make_shared<detail::TypedSidebarSelection<Key>>(binding, keys, navigate);
        };
        return detail::make_sidebar(std::move(recipe_));
    }

  private:
    Sidebar &&add_section(std::string id, std::string title, std::optional<Binding<bool>> open) && {
        const auto index = recipe_.sections.size();
        recipe_.sections.push_back({std::move(id), title, std::move(open)});
        recipe_.rows.push_back({std::move(title), index, {}, true, true, {}});
        current_section_ = index;
        return std::move(*this);
    }
    void add_item(Key key, std::string label, std::optional<Spec> accessory, bool enabled) {
        const auto index = keys_.size();
        keys_.push_back(std::move(key));
        recipe_.rows.push_back(
            {std::move(label), current_section_, index, false, enabled, std::move(accessory)});
    }
    Binding<std::optional<Key>> selection_;
    detail::SidebarRecipe recipe_;
    std::vector<Key> keys_;
    std::optional<std::size_t> current_section_;
    std::function<void(const Key &)> navigate_;
};
} // namespace ui
