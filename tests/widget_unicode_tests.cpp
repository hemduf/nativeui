#include "../src/detail/widget_unicode.hpp"
#include "test_support.hpp"

namespace {
void malformed_bytes_preserve_the_following_text() {
  const std::string input = std::string{"\xff"} + "Ab" + "\xc0\xaf" + "Z";
  const auto repaired = ui::detail::widget_repair_utf8(input);
  NUI_CHECK(repaired == "\xef\xbf\xbd"
                        "Ab"
                        "\xef\xbf\xbd\xef\xbf\xbd"
                        "Z");
  NUI_CHECK(ui::text::utf8_prefix(repaired).has_value());
  NUI_CHECK(ui::detail::widget_initials(repaired) == "AZ");
}
void graphemes_are_not_cut_at_scalar_boundaries() {
  const std::string text = "e\xcc\x81"
                           "👨‍👩‍👧‍👦"
                           "🇫🇷";
  const auto spans = ui::detail::widget_graphemes(text);
  NUI_CHECK(spans.size() == 3);
  NUI_CHECK(text.substr(spans[0].begin, spans[0].end - spans[0].begin) ==
            "e\xcc\x81");
  NUI_CHECK(text.substr(spans[1].begin, spans[1].end - spans[1].begin) ==
            "👨‍👩‍👧‍👦");
  NUI_CHECK(text.substr(spans[2].begin, spans[2].end - spans[2].begin) == "🇫🇷");
  NUI_CHECK(ui::detail::widget_initials("e\xcc\x81mile Martin") ==
            "E\xcc\x81M");
  NUI_CHECK(ui::detail::widget_initials("Élodie—Martin") == "ÉM");
  NUI_CHECK(ui::detail::widget_initials("Camille") == "C");
  NUI_CHECK(ui::detail::widget_initials("  -- 👨‍👩‍👧‍👦 ") ==
            "?");
}
void suite() {
  malformed_bytes_preserve_the_following_text();
  graphemes_are_not_cut_at_scalar_boundaries();
}
} // namespace
int main() { return test::run("widget_unicode", &suite); }
