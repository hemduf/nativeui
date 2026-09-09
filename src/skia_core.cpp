#include <nativeui/nativeui.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <version>

#include "include/core/SkData.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkString.h"
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
namespace {

struct EmbeddedFace {
    std::string alias;
    std::vector<std::byte> source;
    sk_sp<SkTypeface> typeface;
};

using EmbeddedFaces = std::vector<EmbeddedFace>;

#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
using EmbeddedFacesStorage = std::atomic<std::shared_ptr<const EmbeddedFaces>>;

EmbeddedFacesStorage& embedded_faces_storage() {
    // This registry is intentionally process-shared, but published snapshots
    // are immutable. An alias can be registered only once (or repeated with
    // byte-identical data), so one UI/plugin instance cannot replace another
    // live instance's font resource.
    static EmbeddedFacesStorage faces{std::make_shared<const EmbeddedFaces>()};
    return faces;
}

std::shared_ptr<const EmbeddedFaces> embedded_faces_snapshot() {
    return embedded_faces_storage().load(std::memory_order_acquire);
}

void publish_embedded_faces(std::shared_ptr<const EmbeddedFaces> faces) {
    embedded_faces_storage().store(std::move(faces), std::memory_order_release);
}
#else
// Some supported libc++ versions (including the Xcode 16.4 toolchain) do not
// provide the C++20 atomic<shared_ptr<T>> specialization. The standard
// shared_ptr atomic access functions remain available in C++20 and preserve
// the same lock-free-read snapshot contract used by the font registry.
using EmbeddedFacesStorage = std::shared_ptr<const EmbeddedFaces>;

EmbeddedFacesStorage& embedded_faces_storage() {
    // See the atomic specialization above: sharing is process-wide by contract,
    // while existing aliases are immutable after their first registration.
    static EmbeddedFacesStorage faces = std::make_shared<const EmbeddedFaces>();
    return faces;
}

std::shared_ptr<const EmbeddedFaces> embedded_faces_snapshot() {
    return std::atomic_load_explicit(&embedded_faces_storage(), std::memory_order_acquire);
}

void publish_embedded_faces(std::shared_ptr<const EmbeddedFaces> faces) {
    std::atomic_store_explicit(
        &embedded_faces_storage(), std::move(faces), std::memory_order_release);
}
#endif

std::mutex& embedded_faces_mutex() {
    // Serializes publication only. Readers use immutable snapshots and never
    // hold this lock while measuring or painting text.
    static std::mutex mutex;
    return mutex;
}

bool same_font_bytes(const EmbeddedFace& face, std::span<const std::byte> data) {
    return face.source.size() == data.size() &&
           std::equal(data.begin(), data.end(), face.source.begin());
}

sk_sp<SkFontMgr> platform_font_manager() {
    // Immutable process-wide platform service. It contains no NativeUI
    // instance state and is safe to share by contract.
    static const sk_sp<SkFontMgr> manager = [] {
#if defined(__APPLE__)
        return SkFontMgr_New_CoreText(nullptr);
#elif defined(_WIN32)
        return SkFontMgr_New_DirectWrite();
#else
        return SkFontMgr_New_FontConfig(nullptr, SkFontScanner_Make_FreeType());
#endif
    }();
    return manager;
}

SkFontStyle::Slant to_sk_slant(FontSlant slant) {
    switch (slant) {
        case FontSlant::Italic: return SkFontStyle::kItalic_Slant;
        case FontSlant::Oblique: return SkFontStyle::kOblique_Slant;
        case FontSlant::Upright: return SkFontStyle::kUpright_Slant;
    }
    return SkFontStyle::kUpright_Slant;
}

SkFontStyle requested_style(const TextStyle& style) {
    return SkFontStyle(static_cast<int>(style.weight),
                       SkFontStyle::kNormal_Width,
                       to_sk_slant(style.slant));
}

bool has_glyph(const sk_sp<SkTypeface>& typeface, char32_t codepoint) {
    if (!typeface) return false;
    if (codepoint == U'\0') return true;
    return typeface->unicharToGlyph(static_cast<SkUnichar>(codepoint)) != 0;
}

std::string typeface_family(const sk_sp<SkTypeface>& typeface) {
    if (!typeface) return {};
    SkString family;
    typeface->getFamilyName(&family);
    return std::string{family.c_str(), family.size()};
}

struct ResolvedFace {
    sk_sp<SkTypeface> typeface;
    std::string family;
    bool embedded{};
    bool glyph_available{};
    std::size_t priority{};
};

ResolvedFace make_system_face(sk_sp<SkTypeface> typeface,
                              char32_t codepoint,
                              std::size_t priority) {
    if (!typeface) return {};
    const bool glyph = has_glyph(typeface, codepoint);
    auto family = typeface_family(typeface);
    return {std::move(typeface), std::move(family), false, glyph, priority};
}

ResolvedFace match_embedded_family(const EmbeddedFaces& embedded,
                                   std::string_view family,
                                   char32_t codepoint,
                                   std::size_t priority,
                                   bool require_glyph) {
    for (const auto& candidate : embedded) {
        if (candidate.alias != family) continue;
        const bool glyph = has_glyph(candidate.typeface, codepoint);
        if (require_glyph && !glyph) return {};
        return {candidate.typeface, candidate.alias, true, glyph, priority};
    }
    return {};
}

ResolvedFace match_system_family(const sk_sp<SkFontMgr>& manager,
                                 std::string_view family,
                                 const TextStyle& style,
                                 char32_t codepoint,
                                 std::size_t priority,
                                 bool require_glyph) {
    if (!manager || family.empty()) return {};
    const std::string owned_family{family};
    auto face = make_system_face(
        manager->matchFamilyStyle(owned_family.c_str(), requested_style(style)),
        codepoint,
        priority);
    if (require_glyph && !face.glyph_available) return {};
    return face;
}

ResolvedFace match_named_family(const EmbeddedFaces& embedded,
                                const sk_sp<SkFontMgr>& manager,
                                std::string_view family,
                                const TextStyle& style,
                                char32_t codepoint,
                                std::size_t priority,
                                bool require_glyph) {
    if (auto face = match_embedded_family(embedded, family, codepoint, priority, require_glyph);
        face.typeface) {
        return face;
    }
    return match_system_family(manager, family, style, codepoint, priority, require_glyph);
}

ResolvedFace resolve_face(const TextStyle& style,
                          char32_t codepoint,
                          const EmbeddedFaces& embedded,
                          const sk_sp<SkFontMgr>& manager) {
    std::size_t priority = 0;
    if (!style.family.empty()) {
        if (auto face = match_named_family(
                embedded, manager, style.family, style, codepoint, priority, true);
            face.typeface) {
            return face;
        }
        ++priority;
    }

    for (const auto& family : style.fallback_families) {
        if (family.empty()) continue;
        if (auto face = match_named_family(
                embedded, manager, family, style, codepoint, priority, true);
            face.typeface) {
            return face;
        }
        ++priority;
    }

    // Let the platform font manager choose a Unicode-capable system fallback
    // after the explicit NativeUI chain. This keeps multilingual default text
    // useful without exposing CoreText/DirectWrite/Fontconfig to widgets.
    if (manager && codepoint != U'\0') {
        auto face = make_system_face(
            manager->matchFamilyStyleCharacter(
                nullptr,
                requested_style(style),
                nullptr,
                0,
                static_cast<SkUnichar>(codepoint)),
            codepoint,
            priority);
        if (face.typeface && face.glyph_available) return face;
    }

    // A final face is still useful for the font's .notdef glyph when no font
    // on the platform can represent the requested code point.
    priority = 0;
    if (!style.family.empty()) {
        if (auto face = match_named_family(
                embedded, manager, style.family, style, codepoint, priority, false);
            face.typeface) {
            return face;
        }
        ++priority;
    }
    for (const auto& family : style.fallback_families) {
        if (family.empty()) continue;
        if (auto face = match_named_family(
                embedded, manager, family, style, codepoint, priority, false);
            face.typeface) {
            return face;
        }
        ++priority;
    }

    if (manager) {
        return make_system_face(
            manager->matchFamilyStyle(nullptr, requested_style(style)), codepoint, priority);
    }
    return {};
}

struct Utf8Scalar {
    char32_t codepoint{0xFFFD};
    std::size_t next{};
    bool valid{true};
};

Utf8Scalar decode_utf8(std::string_view text, std::size_t offset) {
    if (offset >= text.size()) return {0, text.size()};
    const auto u = [&](std::size_t index) {
        return static_cast<unsigned char>(text[index]);
    };
    const unsigned char a = u(offset);
    if (a < 0x80u) return {static_cast<char32_t>(a), offset + 1};

    const auto continuation = [&](std::size_t index) {
        return index < text.size() && (u(index) & 0xC0u) == 0x80u;
    };

    if ((a & 0xE0u) == 0xC0u && continuation(offset + 1)) {
        const char32_t cp = ((a & 0x1Fu) << 6u) | (u(offset + 1) & 0x3Fu);
        if (cp >= 0x80u) return {cp, offset + 2};
    } else if ((a & 0xF0u) == 0xE0u && continuation(offset + 1) &&
               continuation(offset + 2)) {
        const char32_t cp = ((a & 0x0Fu) << 12u) |
                            ((u(offset + 1) & 0x3Fu) << 6u) |
                            (u(offset + 2) & 0x3Fu);
        if (cp >= 0x800u && !(cp >= 0xD800u && cp <= 0xDFFFu)) {
            return {cp, offset + 3};
        }
    } else if ((a & 0xF8u) == 0xF0u && continuation(offset + 1) &&
               continuation(offset + 2) && continuation(offset + 3)) {
        const char32_t cp = ((a & 0x07u) << 18u) |
                            ((u(offset + 1) & 0x3Fu) << 12u) |
                            ((u(offset + 2) & 0x3Fu) << 6u) |
                            (u(offset + 3) & 0x3Fu);
        if (cp >= 0x10000u && cp <= 0x10FFFFu) return {cp, offset + 4};
    }
    return {0xFFFD, offset + 1, false};
}

bool synthetic_bold(const sk_sp<SkTypeface>& typeface, const TextStyle& style) {
    return style.weight == FontWeight::Bold && typeface &&
           typeface->fontStyle().weight() < SkFontStyle::kSemiBold_Weight;
}

SkFont make_font(const sk_sp<SkTypeface>& typeface, const TextStyle& style, bool embolden) {
    SkFont font(typeface, std::max(0.0f, style.size));
    font.setEdging(SkFont::Edging::kAntiAlias);
    font.setEmbolden(embolden);
    return font;
}

void include_metrics(TextMetrics& output, const SkFontMetrics& input, bool first) {
    if (first) {
        output.ascent = input.fAscent;
        output.descent = input.fDescent;
        output.leading = input.fLeading;
    } else {
        output.ascent = std::min(output.ascent, input.fAscent);
        output.descent = std::max(output.descent, input.fDescent);
        output.leading = std::max(output.leading, input.fLeading);
    }
    output.height = std::max(0.0f, output.descent - output.ascent + output.leading);
}

} // namespace

ResolvedTextLayout resolve_text_layout(std::string_view text, const TextStyle& style) {
    ResolvedTextLayout layout{};
    // Selecting a replacement glyph is not enough: Skia must also receive
    // valid bytes. In particular its macOS UTF-8 glyph conversion can abort
    // for malformed input. Valid text borrows the original buffer, with no
    // repair allocation; malformed bytes each become one Unicode U+FFFD.
    std::size_t copied_until = 0;
    for (std::size_t offset = 0; offset < text.size();) {
        const auto scalar = decode_utf8(text, offset);
        if (!scalar.valid) {
            layout.repaired_text.append(text.substr(copied_until, offset - copied_until));
            layout.repaired_text.append("\xEF\xBF\xBD");
            copied_until = scalar.next;
        }
        offset = scalar.next;
    }
    if (copied_until != 0) {
        layout.repaired_text.append(text.substr(copied_until));
        text = layout.text_bytes(text);
    }
    const auto embedded = embedded_faces_snapshot();
    const auto manager = platform_font_manager();

    if (text.empty()) {
        const auto face = resolve_face(style, U'\0', *embedded, manager);
        if (!face.typeface) return layout;
        const bool embolden = synthetic_bold(face.typeface, style);
        auto font = make_font(face.typeface, style, embolden);
        SkFontMetrics metrics{};
        font.getMetrics(&metrics);
        include_metrics(layout.metrics, metrics, true);
        return layout;
    }

    ResolvedFace current{};
    std::size_t run_begin = 0;
    std::size_t offset = 0;
    bool have_metrics = false;

    const auto flush = [&](std::size_t run_end) {
        if (!current.typeface || run_end <= run_begin) return;
        const bool embolden = synthetic_bold(current.typeface, style);
        auto font = make_font(current.typeface, style, embolden);
        const auto bytes = text.substr(run_begin, run_end - run_begin);
        const float width = font.measureText(bytes.data(), bytes.size(), SkTextEncoding::kUTF8);
        SkFontMetrics metrics{};
        font.getMetrics(&metrics);
        include_metrics(layout.metrics, metrics, !have_metrics);
        have_metrics = true;
        layout.metrics.width += width;
        layout.runs.push_back(
            ResolvedTextRun{run_begin, run_end - run_begin, current.typeface, width, embolden});
    };

    while (offset < text.size()) {
        const auto scalar = decode_utf8(text, offset);
        const bool may_reuse = current.typeface && current.priority == 0 &&
                               has_glyph(current.typeface, scalar.codepoint);
        const auto next_face = may_reuse
                                   ? current
                                   : resolve_face(style, scalar.codepoint, *embedded, manager);

        if (next_face.typeface.get() != current.typeface.get()) {
            flush(offset);
            run_begin = offset;
        }
        current = next_face;
        offset = scalar.next;
    }
    flush(text.size());
    return layout;
}

} // namespace ui::detail

namespace ui {

bool FontManager::register_embedded_font(std::string_view family_alias,
                                         std::span<const std::byte> data) {
    if (family_alias.empty() || data.empty()) return false;

    const std::string alias{family_alias};
    {
        std::lock_guard lock(detail::embedded_faces_mutex());
        const auto current = detail::embedded_faces_snapshot();
        const auto found = std::find_if(current->begin(), current->end(), [&](const auto& face) {
            return face.alias == alias;
        });
        if (found != current->end()) {
            return detail::same_font_bytes(*found, data);
        }
    }

    auto manager = detail::platform_font_manager();
    if (!manager) return false;

    auto bytes = SkData::MakeWithCopy(data.data(), data.size());
    if (!bytes) return false;
    auto typeface = manager->makeFromData(std::move(bytes), 0);
    if (!typeface) return false;

    std::vector<std::byte> source{data.begin(), data.end()};

    std::lock_guard lock(detail::embedded_faces_mutex());
    const auto current = detail::embedded_faces_snapshot();
    const auto found = std::find_if(current->begin(), current->end(), [&](const auto& face) {
        return face.alias == alias;
    });
    if (found != current->end()) {
        // Another instance may have won the registration race while this
        // instance decoded the font. Equal content is idempotent; conflicting
        // content must never replace the already-published resource.
        return detail::same_font_bytes(*found, data);
    }

    auto updated = std::make_shared<detail::EmbeddedFaces>(*current);
    updated->push_back({alias, std::move(source), std::move(typeface)});
    std::shared_ptr<const detail::EmbeddedFaces> published = std::move(updated);
    detail::publish_embedded_faces(std::move(published));
    return true;
}

bool FontManager::has_family(std::string_view family) {
    if (family.empty()) return false;
    const auto embedded = detail::embedded_faces_snapshot();
    for (const auto& candidate : *embedded) {
        if (candidate.alias == family) return true;
    }
    const auto manager = detail::platform_font_manager();
    if (!manager) return false;
    const std::string owned_family{family};
    return manager->matchFamilyStyle(owned_family.c_str(), SkFontStyle()) != nullptr;
}

FontMatch FontManager::match(const TextStyle& style, char32_t codepoint) {
    const auto embedded = detail::embedded_faces_snapshot();
    const auto face = detail::resolve_face(
        style, codepoint, *embedded, detail::platform_font_manager());
    return {face.family, face.embedded, face.glyph_available};
}

TextMetrics TextService::measure(std::string_view text, const TextStyle& style) {
    return detail::resolve_text_layout(text, style).metrics;
}

} // namespace ui
