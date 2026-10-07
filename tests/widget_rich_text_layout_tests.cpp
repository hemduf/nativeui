#include "detail/widget_rich_text.hpp"
#include "test_support.hpp"
#include <nativeui/rich_text.hpp>
#include <set>
namespace {
void shaped_clusters_and_colors_share_geometry() {
  ui::TextStyle style;
  style.size = 24;
  style.family = "serif";
  auto whole = ui::detail::prepare_rich_text_layout({{.text = "office"}}, style,
                                                    500, false);
  auto split = ui::detail::prepare_rich_text_layout(
      {{.text = "of"}, {.text = "fice", .background = ui::Color{1, 0, 0, 1}}},
      style, 500, false);
  NUI_CHECK(whole->paragraphs == split->paragraphs &&
            whole->runs.size() == split->runs.size());
  NUI_CHECK(whole->shaped_paragraphs == 1 && split->shaped_paragraphs == 1 &&
            whole->font_runs == split->font_runs);
  for (std::size_t i = 0; i < whole->runs.size(); ++i) {
    NUI_CHECK(whole->runs[i].glyphs == split->runs[i].glyphs);
    NUI_CHECK(whole->runs[i].clusters == split->runs[i].clusters);
    for (std::size_t j = 0; j < whole->runs[i].positions.size(); ++j) {
      NUI_CHECK(whole->runs[i].positions[j] == split->runs[i].positions[j]);
    }
  }
  NUI_CHECK(whole->preferred.w == split->preferred.w &&
            whole->preferred.h == split->preferred.h);
}
void graphemes_and_ligatures_are_never_split() {
  ui::TextStyle style;
  style.size = 20;
  std::vector<ui::RichTextSpan> spans{
      {.text = "A"},
      {.text = "\xcc\x81 👨"},
      {.text =
           "‍👩‍👧‍👦 abcdefghijklmnopqrstuvwxyz العربية"}};
  auto layout = ui::detail::prepare_rich_text_layout(spans, style, 1, true);
  NUI_CHECK(layout->shaped_paragraphs == 1 && layout->preferred.h > style.size);
  for (const auto &c : layout->clusters) {
    NUI_CHECK(c.begin < c.end &&
              c.end <= layout->paragraphs[c.paragraph].size());
    NUI_CHECK(std::isfinite(c.bounds.x) && std::isfinite(c.bounds.y) &&
              c.bounds.w >= 0 && c.bounds.h >= 0);
    NUI_CHECK(c.begin != 1 && c.begin != 3 + 4 && c.end != 1);
  }
  NUI_CHECK(layout->clusters.front().begin == 0 &&
            layout->clusters.front().end >= 3 &&
            layout->clusters.front().span == 0);
}
void repaired_utf8_rtl_and_inline_hit_use_owned_offsets() {
  int calls{};
  ui::TextStyle style;
  style.size = 20;
  auto layout =
      ui::detail::prepare_rich_text_layout({{.text = std::string("\xff", 1)},
                                            {.id = "arabic",
                                             .text = "العربية abc",
                                             .on_activate = [&] { ++calls; }}},
                                           style, 65, true);
  NUI_CHECK(calls == 0 && layout->paragraphs[0].starts_with("\xef\xbf\xbd") &&
            layout->span_rectangles[1].size() > 1);
  bool rtl{};
  for (const auto &r : layout->runs)
    for (const auto cluster : r.clusters) {
      NUI_CHECK(cluster < layout->paragraphs[r.paragraph].size());
      NUI_CHECK(ui::text::utf8_prefix(
          std::string_view{layout->paragraphs[r.paragraph]}.substr(0,
                                                                   cluster)));
      rtl = rtl || (r.bidi_level & 1U);
    }
  NUI_CHECK(rtl);
  ui::UI tree{ui::RichText{
      std::vector<ui::RichTextSpan>{{.text = "plain "},
                                    {.id = "long",
                                     .text = "action across several lines",
                                     .on_activate = [&] { ++calls; }},
                                    {.text = " ordinary"}}}
                  .style(style)};
  test::MockPlatform platform;
  tree.resize({65, 300});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{65, 300}, 1};
  NUI_CHECK(renderer.render(tree));
  auto local = ui::detail::prepare_rich_text_layout(
      {{.text = "plain "},
       {.id = "long",
        .text = "action across several lines",
        .on_activate = [] {}},
       {.text = " ordinary"}},
      style, 65, true);
  NUI_CHECK(local->span_rectangles[1].size() > 1);
  const auto r = local->span_rectangles[1].back();
  tree.dispatch(test::pointer(ui::InputType::PointerDown, r.x + r.w * .5f,
                              r.y + r.h * .5f),
                platform);
  tree.dispatch(
      test::pointer(ui::InputType::PointerUp, r.x + r.w * .5f, r.y + r.h * .5f),
      platform);
  NUI_CHECK(calls == 1);
  const auto ordinary = local->span_rectangles[2].back();
  tree.dispatch(test::pointer(ui::InputType::PointerDown,
                              ordinary.x + ordinary.w * .5f,
                              ordinary.y + ordinary.h * .5f),
                platform);
  tree.dispatch(test::pointer(ui::InputType::PointerUp,
                              ordinary.x + ordinary.w * .5f,
                              ordinary.y + ordinary.h * .5f),
                platform);
  NUI_CHECK(calls == 1);
}
void fallback_advances_and_rtl_blank_union_are_not_actions() {
  int calls{};
  ui::TextStyle style;
  style.size = 20;
  style.family = "NativeUI-deliberately-unavailable-family";
  style.fallback_families = {"serif"};
  std::vector<ui::RichTextSpan> spans{{.text = "prefix "},
                                      {.id = "rtl",
                                       .text = "العربية العربية العربية abc",
                                       .on_activate = [&] { ++calls; }},
                                      {.text = " suffix"}};
  auto layout = ui::detail::prepare_rich_text_layout(spans, style, 85, true);
  NUI_CHECK(layout->preferred.w <= 85 && layout->preferred.h > style.size);
  float sum{};
  for (const auto &run : layout->runs) {
    NUI_CHECK(run.font.getTypeface());
    NUI_CHECK(std::isfinite(run.advance) && run.advance >= 0);
    sum += run.advance;
  }
  float cluster_sum{};
  for (const auto &cluster : layout->clusters)
    cluster_sum += cluster.bounds.w;
  NUI_CHECK_NEAR(sum, cluster_sum, .05f);
  NUI_CHECK(sum > 20);
  const auto &rectangles = layout->span_rectangles[1];
  NUI_CHECK(rectangles.size() > 1);
  ui::Rect united = rectangles.front();
  for (const auto rect : rectangles) {
    const auto right = std::max(united.x + united.w, rect.x + rect.w),
               bottom = std::max(united.y + united.h, rect.y + rect.h);
    united.x = std::min(united.x, rect.x);
    united.y = std::min(united.y, rect.y);
    united.w = right - united.x;
    united.h = bottom - united.y;
  }
  std::optional<ui::Point> blank;
  for (float y = united.y + 1; y < united.y + united.h && !blank; y += 2)
    for (float x = united.x + 1; x < united.x + united.w; x += 2) {
      ui::Point point{x, y};
      bool contains{};
      for (const auto rect : rectangles)
        contains = contains || rect.contains(point);
      if (!contains) {
        blank = point;
        break;
      }
    }
  NUI_CHECK(blank);
  ui::UI tree{ui::RichText{spans}.style(style)};
  test::MockPlatform platform;
  tree.resize({85, 300});
  tree.activate(platform);
  ui::HeadlessRenderer renderer{{85, 300}, 1};
  NUI_CHECK(renderer.render(tree));
  tree.dispatch(test::pointer(ui::InputType::PointerDown, blank->x, blank->y),
                platform);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, blank->x, blank->y),
                platform);
  NUI_CHECK(calls == 0);
  const auto first = rectangles.front();
  tree.dispatch(test::pointer(ui::InputType::PointerDown,
                              first.x + first.w * .5f, first.y + first.h * .5f),
                platform);
  tree.dispatch(test::pointer(ui::InputType::PointerUp, first.x + first.w * .5f,
                              first.y + first.h * .5f),
                platform);
  NUI_CHECK(calls == 1);
}
void utf8_scalar_split_between_spans_is_repaired_once_as_a_paragraph() {
  auto layout = ui::detail::prepare_rich_text_layout(
      {{.id = "first", .text = std::string("\xc3", 1), .on_activate = [] {}},
       {.id = "second", .text = std::string("\xa9", 1), .on_activate = [] {}}},
      ui::TextStyle{}, 50, true);
  NUI_CHECK(layout->repaired_text == "é" && layout->paragraphs[0] == "é");
  NUI_CHECK(layout->span_text[0] == "é" && layout->span_text[1].empty());
  NUI_CHECK(!layout->span_rectangles[0].empty() &&
            layout->span_rectangles[1].empty());
}
void suite() {
  utf8_scalar_split_between_spans_is_repaired_once_as_a_paragraph();
  shaped_clusters_and_colors_share_geometry();
  graphemes_and_ligatures_are_never_split();
  repaired_utf8_rtl_and_inline_hit_use_owned_offsets();
  fallback_advances_and_rtl_blank_union_are_not_actions();
}
} // namespace
int main() { return test::run("rich_text_layout", suite); }
