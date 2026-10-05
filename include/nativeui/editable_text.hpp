#pragma once
#include <nativeui/text_input.hpp>
namespace ui {
namespace detail {
struct EditableTextControl;
struct EditableTextAccess;
} // namespace detail
class EditableTextController {
public:
  EditableTextController();
  ~EditableTextController();
  EditableTextController(const EditableTextController &) = delete;
  EditableTextController &operator=(const EditableTextController &) = delete;
  void begin();
  void accept();
  void cancel();
  [[nodiscard]] bool editing() const noexcept;

private:
  friend struct detail::EditableTextAccess;
  std::shared_ptr<detail::EditableTextControl> control_;
};
struct EditableTextStyle {
  TextStyle text;
  TextInputStyle text_input;
  std::optional<Color> invalid_color;
  float error_height{};
};
class EditableText {
public:
  using Validator = std::function<std::optional<std::string>(std::string_view)>;
  EditableText(std::string label, Binding<std::string> value);
  EditableText(std::string label, State<std::string> &value);
  EditableText &&controller(std::shared_ptr<EditableTextController> value) &&;
  EditableText &&select_stem(bool value = true) &&;
  EditableText &&validator(Validator value) &&;
  EditableText &&style(EditableTextStyle value) &&;
  Spec spec() &&;

private:
  std::string label_;
  Binding<std::string> value_;
  std::shared_ptr<EditableTextController> controller_;
  bool select_stem_{};
  Validator validator_;
  EditableTextStyle style_;
};
} // namespace ui
