#pragma once
#include <concepts>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <nativeui/combo_popup_style.hpp>
#include <nativeui/component.hpp>
#include <nativeui/state.hpp>
#include <string>
#include <utility>
#include <vector>
namespace ui {
template <class T> struct ComboBoxOption final {
  T value;
  std::string label;
  bool enabled{true};
};
namespace detail {
struct ComboBoxRow {
  std::string label;
  bool enabled{true};
};
struct ComboBoxSnapshot {
  std::vector<ComboBoxRow> rows;
  std::function<std::size_t()> selected_index;
  std::function<std::function<void()>(std::size_t, std::function<bool()>)>
      prepare_selection;
};
struct ComboBoxAdapter {
  ComboBoxSnapshot initial;
  std::function<ComboBoxSnapshot()> snapshot;
  std::function<bool()> valid;
  std::function<std::uint64_t()> revision;
  std::function<std::shared_ptr<void>(std::function<void()>)> observe;
};
[[nodiscard]] Spec combo_box_spec(ComboBoxAdapter, std::string, ComboBoxStyle,
                                  MenuItemStyle);
template <class T>
ComboBoxSnapshot combo_box_snapshot(Binding<T> binding,
                                    std::vector<ComboBoxOption<T>> options) {
  auto owned = std::make_shared<const std::vector<ComboBoxOption<T>>>(
      std::move(options));
  ComboBoxSnapshot result;
  result.rows.reserve(owned->size());
  for (const auto &option : *owned)
    result.rows.push_back({option.label, option.enabled});
  result.selected_index = [owned, binding] {
    // Own the compared value: user equality may reenter the source.
    const auto revision = binding.revision();
    const T selected = binding.snapshot();
    if (!binding.valid() || binding.revision() != revision)
      return std::numeric_limits<std::size_t>::max();
    for (std::size_t index = 0; index < owned->size(); ++index) {
      const bool equal = (*owned)[index].value == selected;
      if (!binding.valid() || binding.revision() != revision)
        return std::numeric_limits<std::size_t>::max();
      if (equal)
        return index;
    }
    return std::numeric_limits<std::size_t>::max();
  };
  result.prepare_selection = [owned, binding](std::size_t index,
                                              std::function<bool()> guard) {
    const auto revision = binding.revision();
    const T expected = binding.snapshot();
    (void)expected;
    if (!binding.valid() || binding.revision() != revision ||
        (guard && !guard()))
      return std::function<void()>{};
    T value = (*owned)[index].value;
    auto permission = [binding, revision, guard = std::move(guard)] {
      return (!guard || guard()) && binding.valid() &&
             binding.revision() == revision;
    };
    if (!permission())
      return std::function<void()>{};
    return std::function<void()>{[binding, permission = std::move(permission),
                                  value = std::move(value)]() mutable {
      if (permission())
        binding.set_if(std::move(value), permission);
    }};
  };
  return result;
}
} // namespace detail
template <class T>
  requires std::copy_constructible<T> && std::equality_comparable<T>
class ComboBox {
public:
  using OptionsProvider = std::function<std::vector<ComboBoxOption<T>>()>;
  ComboBox(Binding<T> selection, std::vector<ComboBoxOption<T>> options)
      : selection_(std::move(selection)), initial_options_(options),
        options_provider_([options = std::move(options)] { return options; }) {}
  ComboBox(State<T> &selection, std::vector<ComboBoxOption<T>> options)
      : ComboBox(selection.binding(), std::move(options)) {}
  ComboBox(Binding<T> selection, OptionsProvider provider)
      : selection_(std::move(selection)),
        options_provider_(std::move(provider)) {
    if (options_provider_)
      initial_options_ = options_provider_();
  }
  ComboBox(State<T> &selection, OptionsProvider provider)
      : ComboBox(selection.binding(), std::move(provider)) {}
  ComboBox &&placeholder(std::string value) && {
    placeholder_ = std::move(value);
    return std::move(*this);
  }
  ComboBox &&style(ComboBoxStyle value) && {
    style_ = std::move(value);
    return std::move(*this);
  }
  ComboBox &&item_style(MenuItemStyle value) && {
    item_style_ = std::move(value);
    return std::move(*this);
  }
  Spec spec() && {
    auto binding = selection_;
    detail::ComboBoxAdapter adapter;
    adapter.initial =
        detail::combo_box_snapshot(binding, std::move(initial_options_));
    adapter.snapshot = [binding, provider = std::move(options_provider_)] {
      return detail::combo_box_snapshot(
          binding, provider ? provider() : std::vector<ComboBoxOption<T>>{});
    };
    adapter.valid = [binding] { return binding.valid(); };
    adapter.revision = [binding] { return binding.revision(); };
    adapter.observe = [binding](std::function<void()> callback) mutable {
      auto subscription = binding.observe(
          [callback = std::move(callback)](const T &) { callback(); });
      return std::static_pointer_cast<void>(
          std::make_shared<typename Binding<T>::Subscription>(
              std::move(subscription)));
    };
    return detail::combo_box_spec(std::move(adapter), std::move(placeholder_),
                                  std::move(style_), std::move(item_style_));
  }

private:
  Binding<T> selection_;
  std::vector<ComboBoxOption<T>> initial_options_;
  OptionsProvider options_provider_;
  std::string placeholder_{"No selection"};
  ComboBoxStyle style_;
  MenuItemStyle item_style_;
};
} // namespace ui
