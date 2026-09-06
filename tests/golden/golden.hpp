#pragma once

#include <nativeui/headless.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace test::golden {

struct Region {
    int x{};
    int y{};
    int w{};
    int h{};

    [[nodiscard]] bool contains(int px, int py) const noexcept {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

struct Image {
    int width{};
    int height{};
    std::vector<std::uint8_t> rgb;

    [[nodiscard]] bool valid() const noexcept {
        return width > 0 && height > 0 &&
               rgb.size() == static_cast<std::size_t>(width) *
                                 static_cast<std::size_t>(height) * 3U;
    }
};

struct CompareOptions {
    int channel_tolerance{0};
    double max_mismatch_ratio{0.0};
    std::vector<Region> compare_regions;
};

struct CompareResult {
    bool matched{};
    std::size_t compared_pixels{};
    std::size_t mismatched_pixels{};
    int max_channel_delta{};
    int first_mismatch_x{-1};
    int first_mismatch_y{-1};

    [[nodiscard]] double mismatch_ratio() const noexcept {
        return compared_pixels
            ? static_cast<double>(mismatched_pixels) /
                  static_cast<double>(compared_pixels)
            : 0.0;
    }
};

inline Image from_renderer(const ui::HeadlessRenderer& renderer) {
    Image out;
    out.width = renderer.pixel_width();
    out.height = renderer.pixel_height();
    const auto& rgba = renderer.rgba_pixels();
    out.rgb.resize(static_cast<std::size_t>(out.width) *
                   static_cast<std::size_t>(out.height) * 3U);

    for (std::size_t src = 0, dst = 0; src + 3 < rgba.size(); src += 4, dst += 3) {
        out.rgb[dst + 0] = rgba[src + 0];
        out.rgb[dst + 1] = rgba[src + 1];
        out.rgb[dst + 2] = rgba[src + 2];
    }
    return out;
}

inline void write_ppm(const std::filesystem::path& path, const Image& image) {
    if (!image.valid()) throw std::invalid_argument("invalid image passed to write_ppm");
    if (const auto parent = path.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("failed to open PPM for writing: " + path.string());
    out << "P6\n" << image.width << ' ' << image.height << "\n255\n";
    out.write(reinterpret_cast<const char*>(image.rgb.data()),
              static_cast<std::streamsize>(image.rgb.size()));
    if (!out) throw std::runtime_error("failed to write PPM: " + path.string());
}

inline std::string read_ppm_token(std::istream& in) {
    std::string token;
    for (;;) {
        int c = in.peek();
        if (c == '#') {
            in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        if (c == EOF) return {};
        if (!std::isspace(static_cast<unsigned char>(c))) break;
        in.get();
    }
    while (in) {
        const int c = in.peek();
        if (c == EOF || std::isspace(static_cast<unsigned char>(c)) || c == '#') break;
        token.push_back(static_cast<char>(in.get()));
    }
    return token;
}

inline Image read_ppm(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("failed to open golden baseline: " + path.string());

    const auto magic = read_ppm_token(in);
    const auto width = read_ppm_token(in);
    const auto height = read_ppm_token(in);
    const auto max_value = read_ppm_token(in);
    if (magic != "P6" || width.empty() || height.empty() || max_value != "255") {
        throw std::runtime_error("unsupported PPM baseline: " + path.string());
    }

    Image image;
    image.width = std::stoi(width);
    image.height = std::stoi(height);
    if (image.width <= 0 || image.height <= 0) {
        throw std::runtime_error("invalid PPM dimensions: " + path.string());
    }

    // PPM requires one whitespace separator after max value. Consume exactly
    // one byte so binary pixel values that happen to be whitespace are never
    // skipped.
    const int separator = in.get();
    if (separator == EOF || !std::isspace(static_cast<unsigned char>(separator))) {
        throw std::runtime_error("invalid PPM header separator: " + path.string());
    }

    image.rgb.resize(static_cast<std::size_t>(image.width) *
                     static_cast<std::size_t>(image.height) * 3U);
    in.read(reinterpret_cast<char*>(image.rgb.data()),
            static_cast<std::streamsize>(image.rgb.size()));
    if (in.gcount() != static_cast<std::streamsize>(image.rgb.size())) {
        throw std::runtime_error("truncated PPM baseline: " + path.string());
    }
    return image;
}

inline bool selected(const CompareOptions& options, int x, int y) {
    if (options.compare_regions.empty()) return true;
    for (const auto& region : options.compare_regions) {
        if (region.contains(x, y)) return true;
    }
    return false;
}

inline CompareResult compare(const Image& expected,
                             const Image& actual,
                             const CompareOptions& options) {
    CompareResult result{};
    if (!expected.valid() || !actual.valid() || expected.width != actual.width ||
        expected.height != actual.height) {
        return result;
    }

    const int tolerance = std::clamp(options.channel_tolerance, 0, 255);
    for (int y = 0; y < expected.height; ++y) {
        for (int x = 0; x < expected.width; ++x) {
            if (!selected(options, x, y)) continue;
            ++result.compared_pixels;
            const auto offset = (static_cast<std::size_t>(y) *
                                 static_cast<std::size_t>(expected.width) +
                                 static_cast<std::size_t>(x)) * 3U;
            bool mismatch = false;
            for (std::size_t channel = 0; channel < 3; ++channel) {
                const int delta = std::abs(static_cast<int>(expected.rgb[offset + channel]) -
                                           static_cast<int>(actual.rgb[offset + channel]));
                result.max_channel_delta = std::max(result.max_channel_delta, delta);
                mismatch = mismatch || delta > tolerance;
            }
            if (mismatch) {
                if (result.first_mismatch_x < 0) {
                    result.first_mismatch_x = x;
                    result.first_mismatch_y = y;
                }
                ++result.mismatched_pixels;
            }
        }
    }

    result.matched = result.compared_pixels > 0 &&
                     result.mismatch_ratio() <= options.max_mismatch_ratio;
    return result;
}

inline Image make_diff_image(const Image& expected,
                             const Image& actual,
                             const CompareOptions& options) {
    Image diff;
    if (!expected.valid() || !actual.valid() || expected.width != actual.width ||
        expected.height != actual.height) {
        return diff;
    }
    diff.width = expected.width;
    diff.height = expected.height;
    diff.rgb.resize(expected.rgb.size(), 0U);
    const int tolerance = std::clamp(options.channel_tolerance, 0, 255);

    for (int y = 0; y < expected.height; ++y) {
        for (int x = 0; x < expected.width; ++x) {
            const auto offset = (static_cast<std::size_t>(y) *
                                 static_cast<std::size_t>(expected.width) +
                                 static_cast<std::size_t>(x)) * 3U;
            if (!selected(options, x, y)) {
                // Dim ignored pixels so compared areas stand out in the artifact.
                for (std::size_t c = 0; c < 3; ++c) {
                    diff.rgb[offset + c] = static_cast<std::uint8_t>(expected.rgb[offset + c] / 5U);
                }
                continue;
            }

            bool mismatch = false;
            for (std::size_t c = 0; c < 3; ++c) {
                mismatch = mismatch ||
                    std::abs(static_cast<int>(expected.rgb[offset + c]) -
                             static_cast<int>(actual.rgb[offset + c])) > tolerance;
            }
            if (mismatch) {
                diff.rgb[offset + 0] = 255;
                diff.rgb[offset + 1] = 0;
                diff.rgb[offset + 2] = 0;
            } else {
                for (std::size_t c = 0; c < 3; ++c) {
                    diff.rgb[offset + c] = static_cast<std::uint8_t>(expected.rgb[offset + c] / 2U);
                }
            }
        }
    }
    return diff;
}

inline bool verify(std::string_view name,
                   const Image& actual,
                   const std::filesystem::path& baseline_dir,
                   const std::filesystem::path& artifact_dir,
                   const CompareOptions& options,
                   bool update) {
    const auto baseline = baseline_dir / (std::string{name} + ".ppm");
    if (update) {
        write_ppm(baseline, actual);
        std::cout << "UPDATED golden " << baseline << '\n';
        return true;
    }

    if (!std::filesystem::exists(baseline)) {
        std::cerr << "MISSING golden baseline: " << baseline
                  << "\nRun nativeui_golden_tests --update-goldens explicitly.\n";
        return false;
    }

    const auto expected = read_ppm(baseline);
    if (!actual.valid() || expected.width != actual.width || expected.height != actual.height) {
        std::filesystem::create_directories(artifact_dir);
        const auto actual_path = artifact_dir / (std::string{name} + ".actual.ppm");
        if (actual.valid()) write_ppm(actual_path, actual);
        std::cerr << "GOLDEN DIMENSION MISMATCH " << name
                  << ": expected=" << expected.width << 'x' << expected.height
                  << " actual=" << actual.width << 'x' << actual.height
                  << "\n  baseline: " << baseline
                  << "\n  actual:   " << actual_path << '\n';
        return false;
    }

    const auto result = compare(expected, actual, options);
    if (result.matched) {
        std::cout << "PASS golden " << name << " (" << result.compared_pixels
                  << " compared pixels, max delta " << result.max_channel_delta << ")\n";
        return true;
    }

    std::filesystem::create_directories(artifact_dir);
    const auto actual_path = artifact_dir / (std::string{name} + ".actual.ppm");
    const auto diff_path = artifact_dir / (std::string{name} + ".diff.ppm");
    write_ppm(actual_path, actual);
    if (const auto diff = make_diff_image(expected, actual, options); diff.valid()) {
        write_ppm(diff_path, diff);
    }

    std::cerr << "GOLDEN MISMATCH " << name
              << ": compared=" << result.compared_pixels
              << " mismatched=" << result.mismatched_pixels
              << " ratio=" << result.mismatch_ratio()
              << " max_delta=" << result.max_channel_delta
              << " first=(" << result.first_mismatch_x << ',' << result.first_mismatch_y << ")"
              << "\n  baseline: " << baseline
              << "\n  actual:   " << actual_path
              << "\n  diff:     " << diff_path << '\n';
    return false;
}

} // namespace test::golden
