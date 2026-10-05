#pragma once

#include <nativeui/component.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/detail/widgets_activation.inc>
#include <nativeui/gesture.hpp>
#include <nativeui/state.hpp>
#include <nativeui/text_edit.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <nativeui/detail/widget_text_paint.hpp>
#include <nativeui/text_input_style.hpp>

namespace ui {
namespace detail {
struct TextInputPolicy;
class TextInputSession;
struct TextInputAccess;
} // namespace detail

class TextInputComponent final : public Component, public detail::ThemeBinding {
public:
  using SubmitCallback = std::function<void(const std::string &)>;

  TextInputComponent(std::string label, Binding<std::string> state,
                     std::string placeholder, std::size_t max_length,
                     SubmitCallback on_submit, TextInputStyle style);

  ~TextInputComponent() override;

  [[nodiscard]] bool focusable() const noexcept override;

  [[nodiscard]] Size measure(const std::vector<ChildMetrics> &) const override;

  void mount(MountContext &ctx) override;

  void unmount(LifecycleContext &context) override;

  void focus_changed(bool focused, FocusContext &ctx) override;

  void deactivate(LifecycleContext &ctx) override;

  EventResult input(const InputEvent &event, InputContext &ctx) override;

  [[nodiscard]] SemanticInfo semantics() const override;

  void paint(PaintContext &p) const override;

private:
  friend struct detail::TextInputAccess;
  friend class detail::TextInputSession;
  EventResult input_impl(const InputEvent &, InputContext &);
  std::shared_ptr<detail::TextInputPolicy> policy_;
  std::shared_ptr<detail::TextInputSession> session_;
  struct PresentationSignature {
    Color field_fill{};
    Color border{};
    Color label{};
    Color text{};
    Color placeholder{};
    Color selection{};
    Color caret{};
    Color composition_underline{};
    float border_width{};
    float corner_radius{};
    float control_width{};
    float control_height{};
    float field_top{};
    float field_height{};
    float horizontal_padding{};
    float content_vertical_inset{};
    float label_offset_y{};
    float label_size{};
    float text_size{};
    float selection_corner_radius{};
    float selection_vertical_inset{};
    float caret_width{};
    float caret_vertical_inset{};
    float composition_underline_width{};
    float composition_underline_inset{};
    FontWeight text_weight{FontWeight::Regular};
    FontSlant text_slant{FontSlant::Upright};
    std::string_view font_family;
    const std::vector<std::string> *fallback_families{};
  };

  static void apply_patch(PresentationSignature &target,
                          const TextInputStylePatch &patch) noexcept;

  [[nodiscard]] PresentationSignature
  presentation_signature(bool focused) const noexcept;

  [[nodiscard]] static bool same_color(Color lhs, Color rhs) noexcept;

  [[nodiscard]] static bool same_fallbacks(const PresentationSignature &lhs,
                                           const PresentationSignature &rhs);

  [[nodiscard]] static bool same_layout(const PresentationSignature &lhs,
                                        const PresentationSignature &rhs);

  [[nodiscard]] static bool same_presentation(const PresentationSignature &lhs,
                                              const PresentationSignature &rhs);

  template <class Context>
  static void invalidate_style_transition(const PresentationSignature &before,
                                          const PresentationSignature &after,
                                          Context &context,
                                          bool require_paint = false);

  [[nodiscard]] VisualState current_visual_state(bool focused) const noexcept;

  [[nodiscard]] ResolvedTextInputStyle resolved_style(bool focused) const;

  [[nodiscard]] static Rect
  field_rect(Rect bounds, const ResolvedTextInputStyle &style) noexcept;

  [[nodiscard]] static Rect
  content_rect(Rect field, const ResolvedTextInputStyle &style) noexcept;

  [[nodiscard]] std::string_view prefix(std::size_t byte_index) const noexcept;

  void copy_selection(InputContext &ctx) const;

  void insert_text(std::string_view raw, InputContext &ctx);

  [[nodiscard]] static TextMotion motion_for(const InputEvent &event) noexcept;

  void move_left(const InputEvent &event, InputContext &ctx);

  void move_right(const InputEvent &event, InputContext &ctx);

  void backspace(const InputEvent &event, InputContext &ctx);

  void delete_forward(const InputEvent &event, InputContext &ctx);

  template <class Context>
  [[nodiscard]] std::size_t index_from_x(Context &ctx, float x) const;

  template <class Context> void ensure_cursor_visible(Context &ctx);

  void commit(InputContext &ctx);

  std::string label_;
  Binding<std::string> state_;
  TextEditModel model_;
  std::string placeholder_;
  SubmitCallback on_submit_;
  TextInputStyle style_;

  bool drag_select_{};
  bool focused_{};
  bool hovered_{};
  bool pressed_{};
  bool caret_visible_{true};
  float scroll_x_{};
  std::string focus_snapshot_;
  std::string pending_ime_commit_;
  Binding<std::string>::Subscription subscription_;
};

class TextInput {
public:
  using SubmitCallback = TextInputComponent::SubmitCallback;

  TextInput(std::string label, Binding<std::string> state);

  TextInput(std::string label, State<std::string> &state);

  TextInput &&placeholder(std::string value) &&;

  TextInput &&max_length(std::size_t value) &&;

  TextInput &&on_submit(SubmitCallback callback) &&;

  TextInput &&style(TextInputStyle value) &&;

  Spec spec() &&;

private:
  std::string label_;
  Binding<std::string> state_;
  std::string placeholder_;
  std::size_t max_length_{256};
  SubmitCallback on_submit_;
  TextInputStyle style_;
};

} // namespace ui
