#include <nativeui/rich_text.hpp>
#include <type_traits>
static_assert(std::is_same_v<decltype(ui::RichTextSpan{}.text), std::string>);
static_assert(std::is_same_v<decltype(ui::RichTextSpan{}.style),
                             std::optional<ui::TextStyle>>);
ui::Spec rich_text_header_probe() {
  return ui::RichText{std::vector<ui::RichTextSpan>{{.text = "text"},
                                                    {.id = "action",
                                                     .text = "action",
                                                     .underline = true,
                                                     .on_activate = [] {}}}}
      .style(ui::TextStyle{})
      .wrap()
      .spec();
}
