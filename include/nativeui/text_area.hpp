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
#include <cstdint>
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
#include <nativeui/text_area_style.hpp>

namespace ui {

class TextAreaComponent final : public Component, public detail::ThemeBinding {
public:
  TextAreaComponent(std::string label, Binding<std::string> state,
                    std::string placeholder, std::size_t max_length,
                    TextAreaStyle style);

  [[nodiscard]] bool focusable() const noexcept override;

  [[nodiscard]] bool uses_retained_checkpoint() const noexcept override;

  [[nodiscard]] Size measure(const std::vector<ChildMetrics> &) const override;

  void mount(MountContext &ctx) override;

  void unmount(LifecycleContext &context) override;

  void focus_changed(bool focused, FocusContext &ctx) override;

  void deactivate(LifecycleContext &ctx) override;

  EventResult input(const InputEvent &event, InputContext &ctx) override;

  [[nodiscard]] SemanticInfo semantics() const override;

  void paint(PaintContext &p) const override;

private:
  struct PublicationState {
    std::size_t depth{};
    std::uint64_t generation{};
    bool refresh_pending{};
  };

  struct LineRange {
    std::size_t begin{};
    std::size_t end{};
  };

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
    float minimum_field_height{};
    float horizontal_padding{};
    float vertical_padding{};
    float label_offset_y{};
    float label_size{};
    float text_size{};
    float line_height{};
    float selection_corner_radius{};
    float selection_vertical_inset{};
    float newline_selection_width{};
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
                          const TextAreaStylePatch &patch) noexcept;

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

  [[nodiscard]] ResolvedTextAreaStyle resolved_style(bool focused) const;

  [[nodiscard]] static TextStyle
  make_text_style(const ResolvedTextAreaStyle &style, float size, Color color);

  [[nodiscard]] static Rect
  field_rect(Rect bounds, const ResolvedTextAreaStyle &style) noexcept;

  [[nodiscard]] static Rect
  content_rect(Rect field, const ResolvedTextAreaStyle &style) noexcept;

  [[nodiscard]] static std::string normalize_multiline(std::string_view input);

  [[nodiscard]] static TextMotion motion_for(const InputEvent &event) noexcept;

  void rebuild_lines();

  void refresh_source(bool source_notification = false);

  void retained_checkpoint() override;

  [[nodiscard]] std::size_t
  line_index_for_cursor(std::size_t cursor) const noexcept;

  [[nodiscard]] std::string_view
  line_text(std::size_t line_index) const noexcept;

  [[nodiscard]] std::string_view
  line_prefix(std::size_t line_index, std::size_t byte_index) const noexcept;

  template <class Context>
  [[nodiscard]] float text_width(Context &ctx, std::string_view text,
                                 const ResolvedTextAreaStyle &style) const;

  template <class Context>
  [[nodiscard]] std::size_t index_from_point(Context &ctx, Point point) const;

  template <class Context> void ensure_cursor_visible(Context &ctx);

  void cursor_moved(InputContext &ctx);

  void copy_selection(InputContext &ctx) const;

  void commit(InputContext &ctx);

  void paint_composition_line(PaintContext &p, std::size_t line_index,
                              float base_x, float top, float center_y,
                              const ResolvedTextAreaStyle &style) const;

  void paint_visible_lines(PaintContext &p, Rect content, bool show_composition,
                           const ResolvedTextAreaStyle &style) const;

  void paint_caret(PaintContext &p, Rect content, bool show_composition,
                   const ResolvedTextAreaStyle &style) const;

  std::string label_;
  Binding<std::string> state_;
  TextEditModel model_;
  std::string placeholder_;
  TextAreaStyle style_;
  std::vector<LineRange> lines_;
  bool drag_select_{};
  bool focused_{};
  bool hovered_{};
  bool pressed_{};
  bool caret_visible_{true};
  float scroll_x_{};
  float scroll_y_{};
  std::string pending_ime_commit_;
  std::string focus_snapshot_;
  std::shared_ptr<PublicationState> publication_{
      std::make_shared<PublicationState>()};
  std::function<void()> source_invalidator_;
  Binding<std::string>::Subscription subscription_;
};

class TextArea {
public:
  TextArea(std::string label, Binding<std::string> state);

  TextArea(std::string label, State<std::string> &state);

  TextArea &&placeholder(std::string value) &&;

  TextArea &&max_length(std::size_t value) &&;

  TextArea &&style(TextAreaStyle value) &&;

  Spec spec() &&;

private:
  std::string label_;
  Binding<std::string> state_;
  std::string placeholder_;
  std::size_t max_length_{};
  TextAreaStyle style_;
};

} // namespace ui
