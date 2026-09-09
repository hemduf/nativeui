#include "example_support.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <string>
#include <string_view>

namespace {
constexpr std::size_t kMaxDisplayedFileBytes = 64U * 1024U;

struct Model {
    std::string status{"Drop a UTF-8 .txt file or text on the panel"};
    std::string payload;
    bool accepted{};
};

struct LoadedTextFile {
    std::string name;
    std::string contents;
    bool truncated{};
    bool supported{true};
};

// Return a complete UTF-8 text prefix, rejecting binary/control bytes. A read
// or display limit may cut the final scalar; only that incomplete tail is
// omitted. This policy belongs to the example, not the generic drop API.
std::optional<std::size_t> text_prefix(std::string_view text, bool truncated = false) {
    for (std::size_t i = 0; i < text.size();) {
        const auto lead = static_cast<unsigned char>(text[i]);
        if (lead < 0x80) {
            if ((lead < 0x20 && lead != '\t' && lead != '\n' && lead != '\r') || lead == 0x7F) {
                return std::nullopt;
            }
            ++i;
            continue;
        }
        const std::size_t count = lead >= 0xC2 && lead <= 0xDF ? 2 :
                                  lead >= 0xE0 && lead <= 0xEF ? 3 :
                                  lead >= 0xF0 && lead <= 0xF4 ? 4 : 0;
        if (!count) return std::nullopt;
        for (std::size_t j = 1; j < count; ++j) {
            if (i + j >= text.size()) return truncated ? std::optional{i} : std::nullopt;
            const auto byte = static_cast<unsigned char>(text[i + j]);
            if (byte < 0x80 || byte > 0xBF ||
                (j == 1 && ((lead == 0xE0 && byte < 0xA0) ||
                            (lead == 0xED && byte > 0x9F) ||
                            (lead == 0xF0 && byte < 0x90) ||
                            (lead == 0xF4 && byte > 0x8F)))) return std::nullopt;
        }
        i += count;
    }
    return text.size();
}

int hex_value(char character) {
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    if (character >= 'A' && character <= 'F') return character - 'A' + 10;
    return -1;
}

std::optional<std::filesystem::path> path_from_file_uri(std::string_view uri) {
    constexpr std::string_view scheme{"file://"};
    if (!uri.starts_with(scheme)) return std::nullopt;

    uri.remove_prefix(scheme.size());
    constexpr std::string_view localhost{"localhost"};
    if (uri.starts_with(localhost)) uri.remove_prefix(localhost.size());
    if (uri.empty() || uri.front() != '/' || uri.find_first_of("?#") != std::string_view::npos) {
        return std::nullopt;
    }

    std::string decoded;
    decoded.reserve(uri.size());
    for (std::size_t i = 0; i < uri.size(); ++i) {
        if (uri[i] == '\0') return std::nullopt;
        if (uri[i] != '%') {
            decoded.push_back(uri[i]);
            continue;
        }

        if (i + 2 >= uri.size()) return std::nullopt;
        const int high = hex_value(uri[i + 1]);
        const int low = hex_value(uri[i + 2]);
        if (high < 0 || low < 0 || (high == 0 && low == 0)) return std::nullopt;
        decoded.push_back(static_cast<char>((high << 4) | low));
        i += 2;
    }

#if defined(_WIN32)
    if (decoded.size() >= 3 && decoded[0] == '/' &&
        std::isalpha(static_cast<unsigned char>(decoded[1])) && decoded[2] == ':') {
        decoded.erase(decoded.begin());
    }
#endif

    std::u8string decoded_utf8;
    decoded_utf8.reserve(decoded.size());
    for (const unsigned char byte : decoded) {
        decoded_utf8.push_back(static_cast<char8_t>(byte));
    }
    return std::filesystem::path{decoded_utf8};
}

std::optional<LoadedTextFile> load_first_text_file(std::string_view uri_list) {
    while (!uri_list.empty()) {
        const auto newline = uri_list.find('\n');
        auto line = uri_list.substr(0, newline);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);

        if (!line.empty() && line.front() != '#') {
            if (const auto path = path_from_file_uri(line)) {
                auto extension = path->extension().u8string();
                for (auto& byte : extension) {
                    if (byte >= u8'A' && byte <= u8'Z') {
                        byte = static_cast<char8_t>(byte + (u8'a' - u8'A'));
                    }
                }
                if (extension != u8".txt") return LoadedTextFile{{}, {}, false, false};
                // Keep this synchronous demo bounded and do not open directories
                // or special files (for example a named pipe waiting for a writer).
                std::error_code error;
                if (!std::filesystem::is_regular_file(*path, error)) return std::nullopt;
                std::ifstream input{*path, std::ios::binary};
                if (!input) return std::nullopt;

                const auto name = path->filename().u8string();
                LoadedTextFile result{{name.begin(), name.end()}, {}, false};
                std::array<char, 4096> buffer{};
                while (input && result.contents.size() < kMaxDisplayedFileBytes) {
                    const auto remaining = kMaxDisplayedFileBytes - result.contents.size();
                    const auto count = static_cast<std::streamsize>(
                        std::min(remaining, buffer.size()));
                    input.read(buffer.data(), count);
                    result.contents.append(buffer.data(), static_cast<std::size_t>(input.gcount()));
                }
                if (input.bad()) return std::nullopt;
                result.truncated = input.peek() != std::char_traits<char>::eof();
                if (const auto size = text_prefix(result.contents, result.truncated)) {
                    result.contents.resize(*size);
                } else {
                    result.contents.clear();
                    result.supported = false;
                }
                return result;
            }
        }

        if (newline == std::string_view::npos) break;
        uri_list.remove_prefix(newline + 1);
    }

    return std::nullopt;
}
}

int main(int argc, char** argv) {
    const bool trace_drops = std::find_if(argv + 1, argv + argc, [](const char* argument) {
        return std::string_view{argument} == "--trace-drops";
    }) != argv + argc;
    Model model;

    auto make_ui = [&] {
        return std::make_unique<ui::UI>(
            ui::Column{
                ui::Header{"T018 / GENERIC DRAG + DROP"},
                ui::Canvas{520.0f, 180.0f, [&](ui::CanvasContext2D& g) {
                    g.fill_rounded_rect({0.0f, 0.0f, g.width(), g.height()}, 12.0f,
                                        model.accepted ? ui::colors::input : ui::colors::panel);
                    g.stroke_rounded_rect({0.0f, 0.0f, g.width(), g.height()}, 12.0f, 2.0f,
                                          model.accepted ? ui::colors::accent : ui::colors::border);
                    g.text({16.0f, 30.0f}, model.status, 12.0f, ui::colors::text);
                    g.text({16.0f, 62.0f}, "Accepted MIME: text/uri-list or text/plain", 10.0f, ui::colors::textMuted);
                    if (!model.payload.empty()) {
                        auto preview = std::string_view{model.payload}.substr(0, 120);
                        preview = preview.substr(0, text_prefix(preview, true).value_or(0));
                        g.text({16.0f, 112.0f}, preview, 10.0f, ui::colors::textMuted);
                    }
                }}.on_input([&](const ui::InputEvent& event, ui::CanvasInputContext& ctx) {
                    if (event.type == ui::InputType::DropOffer) {
                        if (event.offers_drop_type("text/uri-list")) {
                            model.accepted = ctx.accept_drop("text/uri-list");
                            model.status = model.accepted ? "URI drop accepted" : "URI drop rejected by platform";
                        } else if (event.offers_drop_type("text/plain")) {
                            model.accepted = ctx.accept_drop("text/plain");
                            model.status = model.accepted ? "Text drop accepted" : "Text drop rejected by platform";
                        } else {
                            ctx.reject_drop();
                            model.accepted = false;
                            model.status = "Unsupported drop rejected";
                        }
                        ctx.invalidate();
                        return ui::EventResult::Handled;
                    }
                    if (event.type == ui::InputType::DropData) {
                        const std::string data{event.drop_data.begin(), event.drop_data.end()};
                        if (event.drop_type == "text/uri-list") {
                            if (const auto file = load_first_text_file(data)) {
                                model.payload = file->contents;
                                model.accepted = file->supported;
                                model.status = file->supported
                                    ? "Loaded " + file->name + (file->truncated ? " (first 64 KiB)" : "")
                                    : "Unsupported file: expected a UTF-8 .txt file";
                            } else {
                                model.payload.clear();
                                model.accepted = false;
                                model.status = "Could not read dropped file";
                            }
                        } else if (event.drop_type == "text/plain") {
                            const auto preview = std::string_view{data}.substr(0, kMaxDisplayedFileBytes);
                            const auto size = text_prefix(preview, data.size() > preview.size());
                            model.accepted = size.has_value();
                            model.payload = size ? std::string{preview.substr(0, *size)} : std::string{};
                            model.status = size ? "Received UTF-8 text" : "Unsupported drop: expected UTF-8 text";
                        } else {
                            model.accepted = false;
                            model.payload.clear();
                            model.status = "Unsupported drop type";
                        }
                        if (trace_drops) {
                            std::cout << "NativeUI drop: accepted=" << model.accepted
                                      << " text_bytes=" << model.payload.size() << '\n';
                            std::cout.flush();
                        }
                        ctx.invalidate();
                        return ui::EventResult::Handled;
                    }
                    return ui::EventResult::Ignored;
                })
            }.padding(20.0f).gap(14.0f));
    };

    if (example::self_test_requested(argc, argv)) {
        for (const auto bytes : {std::string_view{"\x80"}, std::string_view{"\xC0\xAF"},
                                 std::string_view{"\xED\xA0\x80"}, std::string_view{"\xF4\x90\x80\x80"},
                                 std::string_view{"x\0y", 3}, std::string_view{"\x1B"}}) {
            if (text_prefix(bytes)) return example::fail("binary/invalid UTF-8 text was accepted");
        }
        const std::string utf8 = "Hello \xC3\xA9 \xF0\x9F\x9A\x80\r\n\t";
        if (text_prefix(utf8) != utf8.size()) return example::fail("valid UTF-8 text was rejected");
        const std::string cut_preview = std::string(119, 'x') + "\xC3";
        if (text_prefix(cut_preview, true) != 119 || text_prefix(cut_preview)) {
            return example::fail("UTF-8 preview boundary was not preserved");
        }
        if (path_from_file_uri("file:///tmp/hello%00.txt") ||
            path_from_file_uri("file:///tmp/bad%zz.txt") ||
            path_from_file_uri("file://remote/tmp/hello.txt")) {
            return example::fail("unsafe or malformed file URI was accepted");
        }
        auto tree = make_ui();
        example::Platform platform;
        tree->resize({580.0f, 300.0f});
        tree->activate(platform);
        tree->deactivate(platform); // Finder owns focus while dragging a file.

        ui::InputEvent offer{};
        offer.type = ui::InputType::DropOffer;
        offer.position = {80.0f, 140.0f};
        offer.drop_types = {"text/uri-list"};
        tree->dispatch(offer, platform);
        if (!model.accepted || platform.accepted_drop_type != "text/uri-list") {
            return example::fail("drop offer was not accepted");
        }

        ui::InputEvent data{};
        data.type = ui::InputType::DropData;
        data.position = {90.0f, 150.0f};
        data.drop_type = "text/uri-list";
        const auto test_dir = std::filesystem::temp_directory_path() /
                              ("nativeui-t018-" + std::to_string(std::random_device{}()));
        if (!std::filesystem::create_directory(test_dir)) {
            return example::fail("could not create isolated drop fixture");
        }
        const auto test_path = test_dir / std::filesystem::path{u8"hello caf\u00e9.txt"};
        const auto image_path = test_dir / "image.jpg";
        struct Cleanup {
            std::filesystem::path file;
            std::filesystem::path image;
            ~Cleanup() {
                std::error_code error;
                std::filesystem::remove(file, error);
                std::filesystem::remove(image, error);
                std::filesystem::remove(file.parent_path(), error);
            }
        } cleanup{test_path, image_path};
        const std::string contents = "NativeUI drop file contents\n";
        {
            std::ofstream output{test_path, std::ios::binary};
            output << contents;
            if (!output) return example::fail("could not write drop fixture");
        }

        // Encode the fixture exactly as Finder/Explorer do, including UTF-8,
        // spaces and the leading slash before a Windows drive letter.
        std::string uri = "file://";
#if defined(_WIN32)
        uri += '/';
#endif
        constexpr char digits[] = "0123456789ABCDEF";
        for (const auto byte : test_path.generic_u8string()) {
            const auto value = static_cast<unsigned char>(byte);
            if (value < 128 && (std::isalnum(value) || value == '/' || value == ':' ||
                               value == '.' || value == '-' || value == '_')) {
                uri += static_cast<char>(value);
            } else {
                uri += '%';
                uri += digits[value >> 4];
                uri += digits[value & 15];
            }
        }
        uri += "\r\n";
        const auto decoded = path_from_file_uri(std::string_view{uri}.substr(0, uri.size() - 2));
        if (!decoded || *decoded != test_path) return example::fail("UTF-8 file URI did not round trip");
        data.drop_data.assign(uri.begin(), uri.end());
        tree->dispatch(data, platform);
        if (model.payload != contents) return example::fail("dropped text file contents were not displayed");
        if (model.status != "Loaded hello caf\xC3\xA9.txt") return example::fail("UTF-8 filename was not displayed");

        // Reject binary bytes even when a file has been renamed to .txt.
        {
            std::ofstream output{test_path, std::ios::binary};
            const std::string jpeg{"\xFF\xD8\xFF\xE1", 4};
            output.write(jpeg.data(), static_cast<std::streamsize>(jpeg.size()));
            std::ofstream image{image_path, std::ios::binary};
            image.write(jpeg.data(), static_cast<std::streamsize>(jpeg.size()));
            if (!output || !image) return example::fail("could not write binary drop fixtures");
        }
        tree->dispatch(data, platform);
        if (!model.payload.empty() || model.accepted ||
            model.status != "Unsupported file: expected a UTF-8 .txt file") {
            return example::fail("binary image bytes were accepted as text");
        }
        auto image_data = data;
        const auto image_uri = uri.substr(0, uri.find_last_of('/') + 1) + "image.jpg\r\n";
        image_data.drop_data.assign(image_uri.begin(), image_uri.end());
        tree->dispatch(image_data, platform);
        if (!model.payload.empty() || model.accepted ||
            model.status != "Unsupported file: expected a UTF-8 .txt file") {
            return example::fail("image file was not rejected");
        }
        ui::HeadlessRenderer renderer{{580.0f, 300.0f}, 1.0f};
        if (!renderer.render(*tree)) return example::fail("image rejection did not paint safely");

        // The bounded-read fixture must itself be text, not NUL padding.
        {
            std::ofstream output{test_path, std::ios::binary};
            output << std::string(kMaxDisplayedFileBytes + 1, 'x');
            if (!output) return example::fail("could not write bounded text fixture");
        }
        tree->dispatch(data, platform);
        if (!model.accepted || model.payload.size() != kMaxDisplayedFileBytes ||
            !model.status.ends_with("(first 64 KiB)")) {
            return example::fail("file preview read limit was not enforced");
        }
        {
            std::ofstream output{test_path, std::ios::binary};
            output << std::string(kMaxDisplayedFileBytes - 2, 'x') << "\xF0\x9F\x9A\x80";
            if (!output) return example::fail("could not write bounded UTF-8 fixture");
        }
        tree->dispatch(data, platform);
        if (!model.accepted || model.payload.size() != kMaxDisplayedFileBytes - 2 ||
            !text_prefix(model.payload) || !renderer.render(*tree)) {
            return example::fail("bounded UTF-8 file preview was not safe after an image drop");
        }
        std::filesystem::remove(test_path);
        tree->dispatch(data, platform);
        if (!model.payload.empty() || model.status != "Could not read dropped file") {
            return example::fail("unreadable file retained stale preview contents");
        }
        std::filesystem::create_directory(test_path);
        tree->dispatch(data, platform);
        if (model.status != "Could not read dropped file") {
            return example::fail("directory was treated as a text file");
        }
        auto plain_data = data;
        plain_data.drop_type = "text/plain";
        plain_data.drop_data = {0xFF, 0xD8, 0xFF, 0xE1};
        tree->dispatch(plain_data, platform);
        if (model.accepted || !model.payload.empty() ||
            model.status != "Unsupported drop: expected UTF-8 text") {
            return example::fail("binary plain-text drop was accepted");
        }
        plain_data.drop_data.assign(utf8.begin(), utf8.end());
        tree->dispatch(plain_data, platform);
        if (!model.accepted || model.payload != utf8 || !renderer.render(*tree)) {
            return example::fail("UTF-8 plain-text drop did not recover after binary rejection");
        }
        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree,
        trace_drops ? "NativeUI T018 - Drop diagnostic" : "NativeUI T018 - Drag and Drop",
        {600.0f, 360.0f});
}
