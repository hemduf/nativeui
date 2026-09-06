#include <nativeui/nativeui.hpp>

#include <algorithm>
#include <string_view>

#include "include/core/SkFont.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkTypeface.h"

#if defined(__APPLE__)
#  include "include/ports/SkFontMgr_mac_ct.h"
#elif defined(_WIN32)
#  include "include/ports/SkTypeface_win.h"
#else
#  include "include/ports/SkFontMgr_fontconfig.h"
#  include "include/ports/SkFontScanner_FreeType.h"
#endif

namespace ui::detail {

sk_sp<SkTypeface> default_typeface() {
    static const sk_sp<SkTypeface> typeface = [] {
        sk_sp<SkFontMgr> manager;
#if defined(__APPLE__)
        manager = SkFontMgr_New_CoreText(nullptr);
#elif defined(_WIN32)
        manager = SkFontMgr_New_DirectWrite();
#else
        manager = SkFontMgr_New_FontConfig(nullptr, SkFontScanner_Make_FreeType());
#endif
        return manager ? manager->matchFamilyStyle(nullptr, SkFontStyle()) : nullptr;
    }();
    return typeface;
}

} // namespace ui::detail

namespace ui {

TextMetrics TextService::measure(std::string_view text, const TextStyle& style) {
    const float size = std::max(0.0f, style.size);
    SkFont font(detail::default_typeface(), size);
    font.setEdging(SkFont::Edging::kAntiAlias);
    font.setEmbolden(style.weight == FontWeight::Bold);

    SkFontMetrics sk_metrics{};
    font.getMetrics(&sk_metrics);

    TextMetrics metrics{};
    metrics.width = text.empty()
                        ? 0.0f
                        : font.measureText(text.data(), text.size(), SkTextEncoding::kUTF8);
    metrics.ascent = sk_metrics.fAscent;
    metrics.descent = sk_metrics.fDescent;
    metrics.leading = sk_metrics.fLeading;
    metrics.height = std::max(0.0f,
                              sk_metrics.fDescent - sk_metrics.fAscent + sk_metrics.fLeading);
    return metrics;
}

} // namespace ui
