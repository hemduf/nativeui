#include "example_support.hpp"

#include <string>

namespace {

std::unique_ptr<ui::UI> make_ui() {
    ui::TextStyle title{};
    title.size = 24.0f;
    title.weight = ui::FontWeight::Bold;
    title.color = ui::colors::accent;

    ui::TextStyle multilingual{};
    multilingual.size = 22.0f;
    multilingual.fallback_families = {};

    const auto default_face = ui::FontManager::match(multilingual, U'A');
    const std::string family = default_face ? default_face.family : std::string{"system default"};

    return std::make_unique<ui::UI>(
        ui::Column{
            ui::Header{"T027 / FONT MANAGER + FALLBACK"},
            ui::Label{"Platform-neutral font selection"}.style(title),
            ui::Label{"Resolved default family: " + family}
                .size(14.0f)
                .color(ui::colors::textMuted),
            ui::Label{"NativeUI / \xCE\xA9 / \xE6\x97\xA5\xE6\x9C\xAC"}
                .style(multilingual),
            ui::Label{"Bold + italic style request"}
                .size(18.0f)
                .bold()
                .italic()
        }.padding(20.0f).gap(14.0f));
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) {
        ui::TextStyle style{};
        const auto default_face = ui::FontManager::match(style, U'A');
        if (!default_face || default_face.family.empty()) {
            return example::fail("FontManager could not resolve the platform default face");
        }

        ui::TextStyle fallback{};
        fallback.family = "NativeUI intentionally missing family";
        fallback.fallback_families = {default_face.family};
        const auto selected = ui::FontManager::match(fallback, U'A');
        if (!selected || !selected.glyph_available) {
            return example::fail("Named fallback chain could not resolve a Latin glyph");
        }

        const auto metrics = ui::TextService::measure(
            "NativeUI / \xCE\xA9 / \xE6\x97\xA5\xE6\x9C\xAC", fallback);
        if (!(metrics.width > 0.0f && metrics.height > 0.0f)) {
            return example::fail("Fallback text measurement returned empty metrics");
        }
        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree, "NativeUI T027 - Fonts", {680.0f, 360.0f});
}
