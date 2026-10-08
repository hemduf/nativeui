#pragma once
#include <concepts>
#include <functional>
#include <memory>
#include <nativeui/state.hpp>
#include <nativeui/toggle_group.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace ui {
template <class T> struct SegmentOption {
  T value;
  std::string label;
  bool enabled{true};
};
struct SegmentedControlStyle : ToggleGroupStyle {};
namespace detail {
struct SegmentedSelectionSnapshot {
  std::vector<bool> selected;
  std::uint64_t revision{};
};
class SegmentedSubscription {
public:
  virtual ~SegmentedSubscription() = default;
};
class SegmentedSelection {
public:
  virtual ~SegmentedSelection() = default;
  virtual bool valid() const noexcept = 0;
  virtual std::uint64_t revision() const noexcept = 0;
  virtual std::optional<SegmentedSelectionSnapshot>
  snapshot(const std::function<bool()> &current) const = 0;
  virtual void select(std::size_t index,
                      const std::function<bool()> &current) = 0;
  virtual std::unique_ptr<SegmentedSubscription>
  observe(std::function<void()> callback) = 0;
};
using SegmentedSelectionFactory =
    std::function<std::unique_ptr<SegmentedSelection>()>;
Spec make_segmented_control_spec(std::string label,
                                 std::vector<std::string> labels,
                                 std::vector<bool> enabled,
                                 SegmentedControlStyle style,
                                 SegmentedSelectionFactory selection);
template <class T>
class TypedSegmentedSubscription final : public SegmentedSubscription {
public:
  explicit TypedSegmentedSubscription(
      typename Binding<T>::Subscription subscription)
      : subscription_(std::move(subscription)) {}

private:
  typename Binding<T>::Subscription subscription_;
};
template <class T>
class TypedSegmentedSelection final : public SegmentedSelection {
public:
  TypedSegmentedSelection(
      Binding<T> source,
      std::shared_ptr<const std::vector<SegmentOption<T>>> options)
      : source_(std::move(source)), options_(std::move(options)) {}
  bool valid() const noexcept override { return source_.valid(); }
  std::uint64_t revision() const noexcept override {
    return source_.revision();
  }
  std::optional<SegmentedSelectionSnapshot>
  snapshot(const std::function<bool()> &current) const override {
    const auto revision = source_.revision();
    if (!source_.valid() || (current && !current()))
      return {};
    auto owned = source_.snapshot();
    if (!source_.valid() || source_.revision() != revision ||
        (current && !current()))
      return {};
    SegmentedSelectionSnapshot result;
    result.revision = revision;
    result.selected.resize(options_->size());
    for (std::size_t i = 0; i < options_->size(); ++i) {
      const bool selected = owned == (*options_)[i].value;
      if (!source_.valid() || source_.revision() != revision ||
          (current && !current()))
        return {};
      result.selected[i] = selected;
    }
    return result;
  }
  void select(std::size_t index,
              const std::function<bool()> &current) override {
    if (index >= options_->size() || !source_.valid() ||
        (current && !current()))
      return;
    const auto revision = source_.revision();
    T candidate = (*options_)[index].value;
    if (!source_.valid() || source_.revision() != revision ||
        (current && !current()))
      return;
    auto source = source_;
    source.set_if(std::move(candidate), [source, revision, current] {
      return source.valid() && source.revision() == revision &&
             (!current || current());
    });
  }
  std::unique_ptr<SegmentedSubscription>
  observe(std::function<void()> callback) override {
    return std::make_unique<TypedSegmentedSubscription<T>>(
        source_.observe([callback = std::move(callback)](const T &) {
          if (callback)
            callback();
        }));
  }

private:
  Binding<T> source_;
  std::shared_ptr<const std::vector<SegmentOption<T>>> options_;
};
} // namespace detail
template <class T>
  requires std::copy_constructible<T> && std::equality_comparable<T>
class SegmentedControl {
public:
  SegmentedControl(std::string label, Binding<T> selected,
                   std::vector<SegmentOption<T>> options)
      : label_(std::move(label)), selected_(std::move(selected)),
        options_(std::move(options)) {}
  SegmentedControl(std::string label, State<T> &selected,
                   std::vector<SegmentOption<T>> options)
      : SegmentedControl(std::move(label), selected.binding(),
                         std::move(options)) {}
  SegmentedControl &&style(SegmentedControlStyle value) && {
    style_ = std::move(value);
    return std::move(*this);
  }
  Spec spec() && {
    for (std::size_t i = 0; i < options_.size(); ++i)
      for (std::size_t j = 0; j < i; ++j)
        if (options_[i].value == options_[j].value)
          throw std::invalid_argument(
              "SegmentedControl option values must be unique");
    auto options = std::make_shared<const std::vector<SegmentOption<T>>>(
        std::move(options_));
    std::vector<std::string> labels;
    std::vector<bool> enabled;
    labels.reserve(options->size());
    enabled.reserve(options->size());
    for (const auto &option : *options) {
      labels.push_back(option.label);
      enabled.push_back(option.enabled);
    }
    auto source = std::move(selected_);
    detail::SegmentedSelectionFactory factory = [source, options] {
      return std::make_unique<detail::TypedSegmentedSelection<T>>(source,
                                                                  options);
    };
    return detail::make_segmented_control_spec(
        std::move(label_), std::move(labels), std::move(enabled),
        std::move(style_), std::move(factory));
  }

private:
  std::string label_;
  Binding<T> selected_;
  std::vector<SegmentOption<T>> options_;
  SegmentedControlStyle style_;
};
template <class T>
SegmentedControl(std::string, Binding<T>, std::vector<SegmentOption<T>>)
    -> SegmentedControl<T>;
template <class T>
SegmentedControl(std::string, State<T> &, std::vector<SegmentOption<T>>)
    -> SegmentedControl<T>;
} // namespace ui
