#include "example_support.hpp"

#include <string>

namespace {
struct Model {
    std::string status{"Drop a file or text on the panel"};
    std::string payload;
    bool accepted{};
};
}

int main(int argc, char** argv) {
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
                        const auto preview = model.payload.substr(0, 120);
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
                        model.payload.assign(event.drop_data.begin(), event.drop_data.end());
                        model.status = "Received " + event.drop_type + " at " +
                            std::to_string(static_cast<int>(event.position.x)) + "," +
                            std::to_string(static_cast<int>(event.position.y));
                        ctx.invalidate();
                        return ui::EventResult::Handled;
                    }
                    return ui::EventResult::Ignored;
                })
            }.padding(20.0f).gap(14.0f));
    };

    if (example::self_test_requested(argc, argv)) {
        auto tree = make_ui();
        example::Platform platform;
        tree->resize({580.0f, 300.0f});
        tree->activate(platform);

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
        const std::string uri = "file:///tmp/snare.wav\n";
        data.drop_data.assign(uri.begin(), uri.end());
        tree->dispatch(data, platform);
        if (model.payload != uri) return example::fail("drop payload was not delivered");
        return 0;
    }

    auto tree = make_ui();
    return example::run_window(*tree, "NativeUI T018 - Drag and Drop", {600.0f, 360.0f});
}
