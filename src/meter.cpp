#include <nativeui/meter.hpp>

namespace ui {

namespace detail {
namespace {
int meter_level(float value,
                const std::optional<MeterLevels> &levels) noexcept {
  if (!levels || levels->warning == levels->critical)
    return 0;
  if (levels->critical > levels->warning) {
    if (value >= levels->critical)
      return 2;
    if (value >= levels->warning)
      return 1;
  } else {
    if (value <= levels->critical)
      return 2;
    if (value <= levels->warning)
      return 1;
  }
  return 0;
}
} // namespace

Color meter_fill_color(float value, Color normal,
                       const std::optional<MeterLevels> &levels, Color warning,
                       Color critical) noexcept {
  switch (meter_level(value, levels)) {
  case 1:
    return warning;
  case 2:
    return critical;
  default:
    return normal;
  }
}

std::string meter_level_description(float value,
                                    const std::optional<MeterLevels> &levels) {
  if (!levels || levels->warning == levels->critical)
    return {};
  switch (meter_level(value, levels)) {
  case 1:
    return "avertissement";
  case 2:
    return "critique";
  default:
    return "normal";
  }
}
} // namespace detail

Meter::Meter(State<float> &state, float minimum, float maximum)
    : Meter(state.binding(), minimum, maximum) {}

Meter::Meter(Binding<float> state, float minimum, float maximum)
    : state_(std::move(state)), minimum_(minimum), maximum_(maximum) {}

Meter &&Meter::levels(MeterLevels value) && {
  if (!std::isfinite(value.warning) || !std::isfinite(value.critical) ||
      value.warning < minimum_ || value.warning > maximum_ ||
      value.critical < minimum_ || value.critical > maximum_)
    throw std::invalid_argument(
        "Meter levels must be finite and within its range");
  levels_ = value;
  return std::move(*this);
}

Meter &&Meter::threshold_colors(Color warning, Color critical) && {
  warning_ = warning;
  critical_ = critical;
  return std::move(*this);
}

Meter &&Meter::orientation(ProgressOrientation value) && {
  orientation_ = value;
  return std::move(*this);
}

Meter &&Meter::formatter(Formatter value) && {
  formatter_ = std::move(value);
  return std::move(*this);
}

Meter &&Meter::style(MeterStyle value) && {
  style_ = std::move(value);
  return std::move(*this);
}

Spec Meter::spec() && {
  auto state = std::move(state_);
  const float minimum = minimum_;
  const float maximum = maximum_;
  const auto orientation = orientation_;
  const auto levels = levels_;
  const auto warning = warning_;
  const auto critical = critical_;
  auto formatter = std::move(formatter_);
  auto style = std::move(style_);
  return Spec{[state = std::move(state), minimum, maximum, orientation, levels,
               warning, critical, formatter = std::move(formatter),
               style = std::move(style)]() mutable {
                return std::make_unique<detail::BoundedDisplayComponent>(
                    std::move(state), minimum, maximum, orientation,
                    std::move(formatter), std::move(style), levels, warning,
                    critical);
              },
              {}};
}

} // namespace ui
