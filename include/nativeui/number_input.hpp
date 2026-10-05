#pragma once
#include <nativeui/component.hpp>
#include <nativeui/stepper.hpp>
#include <nativeui/text_input.hpp>

namespace ui {
struct NumberInputStyle {
  TextInputStyle text_input;
  StepperStyle stepper;
  double gap{4.0};
  std::optional<Color> invalid_color;
  std::string invalid_message{"Valeur invalide"};
};
class NumberInput {
public:
  NumberInput(std::string label, Binding<double> value);
  NumberInput(std::string label, State<double> &value);
  NumberInput &&range(double minimum, double maximum) &&;
  NumberInput &&step(double value) &&;
  NumberInput &&precision(unsigned digits) &&;
  NumberInput &&on_submit(std::function<void(double)> callback) &&;
  NumberInput &&style(NumberInputStyle value) &&;
  Spec spec() &&;

private:
  std::string label_;
  Binding<double> value_;
  double minimum_{};
  double maximum_{100.0};
  double step_{1.0};
  std::optional<unsigned> precision_;
  std::function<void(double)> on_submit_;
  NumberInputStyle style_;
};
} // namespace ui
