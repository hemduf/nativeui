#include <nativeui/image.hpp>
#include <nativeui/paint.hpp>
#include <nativeui/svg.hpp>

#include "include/core/SkCanvas.h"
#include "include/core/SkStream.h"
#include "modules/svg/include/SkSVGDOM.h"

#include <algorithm>
#include <cmath>
#include <locale>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace ui::detail {

struct SvgData {
    sk_sp<SkSVGDOM> dom;
    Size intrinsic{};
};

struct SvgAccess {
    [[nodiscard]] static const std::shared_ptr<const SvgData>& data(const SvgIcon& icon) noexcept {
        return icon.data_;
    }
};

namespace {

[[nodiscard]] bool drawable_rect(Rect rect) noexcept {
    return std::isfinite(rect.x) && std::isfinite(rect.y) &&
           std::isfinite(rect.w) && std::isfinite(rect.h) &&
           rect.w > 0.0f && rect.h > 0.0f;
}

[[nodiscard]] bool drawable_size(Size size) noexcept {
    return std::isfinite(size.w) && std::isfinite(size.h) &&
           size.w > 0.0f && size.h > 0.0f;
}

[[nodiscard]] bool xml_space(char ch) noexcept {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

[[nodiscard]] std::optional<Size> parse_viewbox_size(std::span<const std::byte> encoded) {
    const std::string_view xml{reinterpret_cast<const char*>(encoded.data()), encoded.size()};
    std::size_t svg_pos = 0;
    for (;;) {
        svg_pos = xml.find("<svg", svg_pos);
        if (svg_pos == std::string_view::npos) return std::nullopt;
        const std::size_t after_name = svg_pos + 4;
        if (after_name == xml.size() || xml_space(xml[after_name]) ||
            xml[after_name] == '>' || xml[after_name] == '/') {
            break;
        }
        svg_pos = after_name;
    }

    std::size_t tag_end = svg_pos + 4;
    char quote = '\0';
    for (; tag_end < xml.size(); ++tag_end) {
        const char ch = xml[tag_end];
        if (quote != '\0') {
            if (ch == quote) quote = '\0';
            continue;
        }
        if (ch == '\'' || ch == '"') {
            quote = ch;
        } else if (ch == '>') {
            break;
        }
    }
    if (tag_end == xml.size()) return std::nullopt;

    const std::string_view tag = xml.substr(svg_pos + 4, tag_end - (svg_pos + 4));
    std::size_t attr_pos = 0;
    while ((attr_pos = tag.find("viewBox", attr_pos)) != std::string_view::npos) {
        const bool left_boundary = attr_pos == 0 || xml_space(tag[attr_pos - 1]);
        const std::size_t name_end = attr_pos + 7;
        const bool right_boundary = name_end == tag.size() || xml_space(tag[name_end]) || tag[name_end] == '=';
        if (!left_boundary || !right_boundary) {
            attr_pos = name_end;
            continue;
        }

        std::size_t value_pos = name_end;
        while (value_pos < tag.size() && xml_space(tag[value_pos])) ++value_pos;
        if (value_pos == tag.size() || tag[value_pos] != '=') return std::nullopt;
        ++value_pos;
        while (value_pos < tag.size() && xml_space(tag[value_pos])) ++value_pos;
        if (value_pos == tag.size() || (tag[value_pos] != '\'' && tag[value_pos] != '"')) {
            return std::nullopt;
        }

        const char value_quote = tag[value_pos++];
        const std::size_t value_end = tag.find(value_quote, value_pos);
        if (value_end == std::string_view::npos) return std::nullopt;

        std::string numbers{tag.substr(value_pos, value_end - value_pos)};
        for (char& ch : numbers) {
            if (ch == ',') ch = ' ';
        }

        std::istringstream input{numbers};
        input.imbue(std::locale::classic());
        float components[4]{};
        for (float& component : components) {
            if (!(input >> component)) return std::nullopt;
        }
        input >> std::ws;
        if (!input.eof() || !std::isfinite(components[0]) || !std::isfinite(components[1]) ||
            !std::isfinite(components[2]) || !std::isfinite(components[3]) ||
            components[2] <= 0.0f || components[3] <= 0.0f) {
            return std::nullopt;
        }
        return Size{components[2], components[3]};
    }
    return std::nullopt;
}

[[nodiscard]] Size resolved_intrinsic_size(SkSVGDOM& dom,
                                           std::span<const std::byte> encoded) {
    const auto sk_size = dom.containerSize();
    const Size intrinsic{sk_size.width(), sk_size.height()};
    if (drawable_size(intrinsic)) return intrinsic;

    // Avoid reading SkSVGSVG inline data members across the prebuilt Skia ABI
    // boundary. The packaged SVG implementation and client compiler can differ
    // in STL layout details. Extract only root viewBox metadata from the source
    // bytes, then give the parsed DOM a stable container once at load time.
    const auto viewbox = parse_viewbox_size(encoded);
    if (!viewbox || !drawable_size(*viewbox)) return {};
    dom.setContainerSize(SkSize::Make(viewbox->w, viewbox->h));
    return *viewbox;
}

} // namespace

void draw_svg(Painter& painter, const SvgIcon& icon, Rect destination) {
    const auto& data = SvgAccess::data(icon);
    if (!data || !data->dom || !drawable_size(data->intrinsic) || !drawable_rect(destination)) {
        return;
    }

    const float scale = std::min(destination.w / data->intrinsic.w,
                                 destination.h / data->intrinsic.h);
    if (!std::isfinite(scale) || scale <= 0.0f) return;

    const float rendered_w = data->intrinsic.w * scale;
    const float rendered_h = data->intrinsic.h * scale;
    const float offset_x = (destination.w - rendered_w) * 0.5f;
    const float offset_y = (destination.h - rendered_h) * 0.5f;

    auto& canvas = painter.canvas();
    canvas.save();
    canvas.clipRect(SkRect::MakeXYWH(
        destination.x, destination.y, destination.w, destination.h));
    canvas.translate(destination.x + offset_x, destination.y + offset_y);
    canvas.scale(scale, scale);
    data->dom->render(&canvas);
    canvas.restore();
}

} // namespace ui::detail

namespace ui {

SvgIcon SvgIcon::parse(std::span<const std::byte> encoded) {
    if (encoded.empty()) return {};

    SkMemoryStream stream(encoded.data(), encoded.size(), true);
    auto dom = SkSVGDOM::MakeFromStream(stream);
    if (!dom) return {};

    const Size intrinsic = detail::resolved_intrinsic_size(*dom, encoded);
    if (!detail::drawable_size(intrinsic)) return {};

    auto data = std::make_shared<detail::SvgData>();
    data->dom = std::move(dom);
    data->intrinsic = intrinsic;
    return SvgIcon{std::move(data)};
}

Size SvgIcon::intrinsic_size() const noexcept {
    return data_ ? data_->intrinsic : Size{};
}

struct SvgCache::Impl {
    explicit Impl(ResourceProvider& provider_in)
        : provider(&provider_in) {}

    ResourceProvider* provider{};
    std::unordered_map<std::string, SvgLoadResult> entries;
};

SvgCache::SvgCache(ResourceProvider& provider)
    : impl_(std::make_unique<Impl>(provider)) {}

SvgCache::~SvgCache() = default;
SvgCache::SvgCache(SvgCache&&) noexcept = default;
SvgCache& SvgCache::operator=(SvgCache&&) noexcept = default;

SvgLoadResult SvgCache::load(std::string_view resource_id) {
    if (!impl_ || !impl_->provider) {
        return {{}, SvgLoadError::NotFound};
    }

    const std::string key{resource_id};
    if (const auto existing = impl_->entries.find(key); existing != impl_->entries.end()) {
        return existing->second;
    }

    SvgLoadResult result;
    const auto encoded = impl_->provider->load(resource_id);
    if (!encoded) {
        result.error = SvgLoadError::NotFound;
    } else {
        result.icon = SvgIcon::parse(*encoded);
        if (!result.icon.valid()) {
            result.error = SvgLoadError::ParseFailed;
        }
    }

    impl_->entries.emplace(key, result);
    return result;
}

std::size_t SvgCache::size() const noexcept {
    return impl_ ? impl_->entries.size() : 0U;
}

void SvgCache::clear() {
    if (impl_) impl_->entries.clear();
}

} // namespace ui
