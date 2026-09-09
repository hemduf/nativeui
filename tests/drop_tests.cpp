#include "test_support.hpp"

#include <memory>
#include <string>

namespace {

struct DropState {
    int offers{};
    int data_events{};
    ui::Point offer_position{};
    ui::Point data_position{};
    std::string data_type;
    std::string payload;
};

class DropTargetComponent final : public ui::Component {
public:
    explicit DropTargetComponent(std::shared_ptr<DropState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {220.0f, 100.0f};
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext& ctx) override {
        if (event.type == ui::InputType::DropOffer) {
            ++state_->offers;
            state_->offer_position = event.position;
            if (event.offers_drop_type("text/uri-list")) {
                NUI_CHECK(ctx.accept_drop("text/uri-list"));
            } else {
                ctx.reject_drop();
            }
            return ui::EventResult::Handled;
        }

        if (event.type == ui::InputType::DropData) {
            ++state_->data_events;
            state_->data_position = event.position;
            state_->data_type = event.drop_type;
            state_->payload.assign(event.drop_data.begin(), event.drop_data.end());
            return ui::EventResult::Handled;
        }
        return ui::EventResult::Ignored;
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::shared_ptr<DropState> state_;
};

class DropTarget {
public:
    explicit DropTarget(std::shared_ptr<DropState> state) : state_(std::move(state)) {}
    ui::Spec spec() && {
        auto state = std::move(state_);
        return ui::Spec{[state = std::move(state)] {
            return std::make_unique<DropTargetComponent>(state);
        }, {}};
    }
private:
    std::shared_ptr<DropState> state_;
};

void suite() {
    auto state = std::make_shared<DropState>();
    ui::UI tree{DropTarget{state}};
    test::MockPlatform platform;
    tree.resize({300.0f, 160.0f});
    tree.activate(platform);

    ui::InputEvent offer{};
    offer.type = ui::InputType::DropOffer;
    offer.position = {70.0f, 40.0f};
    offer.drop_types = {"text/plain", "text/uri-list"};
    NUI_CHECK(tree.dispatch(offer, platform) == ui::EventResult::Handled);
    NUI_CHECK(state->offers == 1);
    NUI_CHECK(platform.drop_accept_count == 1);
    NUI_CHECK(platform.accepted_drop_type == "text/uri-list");
    NUI_CHECK(platform.paste_request_count == 0);

    ui::InputEvent data{};
    data.type = ui::InputType::DropData;
    data.position = {80.0f, 55.0f};
    data.drop_type = "text/uri-list";
    const std::string uri = "file:///tmp/kick.wav\n";
    data.drop_data.assign(uri.begin(), uri.end());
    NUI_CHECK(tree.dispatch(data, platform) == ui::EventResult::Handled);
    NUI_CHECK(state->data_events == 1);
    NUI_CHECK(state->data_type == "text/uri-list");
    NUI_CHECK(state->payload == uri);

    // A background drop does not implicitly activate keyboard/command input.
    auto key_probe = std::make_shared<test::ProbeState>();
    auto background_state = std::make_shared<DropState>();
    test::MockPlatform background_platform;
    ui::UI background{ui::Column{test::Probe{key_probe}, DropTarget{background_state}}};
    background.resize({300.0f, 180.0f});
    background.activate(background_platform);
    background.deactivate(background_platform);
    const int focus_in = key_probe->focus_in;
    const int focus_out = key_probe->focus_out;
    auto background_offer = offer;
    background_offer.position = {80.0f, 80.0f};
    NUI_CHECK(background.dispatch(background_offer, background_platform) == ui::EventResult::Handled);
    NUI_CHECK(background.dispatch(test::key(ui::Key::Space), background_platform) == ui::EventResult::Ignored);
    NUI_CHECK(background.dispatch(test::text("ignored"), background_platform) == ui::EventResult::Ignored);
    NUI_CHECK(key_probe->key_events == 0);
    NUI_CHECK(key_probe->focus_in == focus_in && key_probe->focus_out == focus_out);
    NUI_CHECK(background_state->offers == 1);
    NUI_CHECK(background_state->data_events == 0);
    NUI_CHECK(state->data_events == 1);

    // Non-focusable custom components still receive drops through deepest-node
    // hit testing, which is independent from keyboard focusability.
    NUI_CHECK(state->offer_position.x == 70.0f);
    NUI_CHECK(state->data_position.y == 55.0f);

    ui::InputEvent unknown{};
    unknown.type = ui::InputType::DropOffer;
    unknown.position = {20.0f, 20.0f};
    unknown.drop_types = {"application/octet-stream"};
    tree.dispatch(unknown, platform);
    NUI_CHECK(platform.drop_reject_count == 1);

    // Finder (and any external drag source) owns keyboard focus during a
    // drag. Losing window focus must not disable an otherwise live target.
    tree.deactivate(platform);
    NUI_CHECK(tree.dispatch(offer, platform) == ui::EventResult::Handled);
    NUI_CHECK(state->offers == 3);
    NUI_CHECK(platform.drop_accept_count == 2);
    NUI_CHECK(tree.dispatch(data, platform) == ui::EventResult::Handled);
    NUI_CHECK(state->data_events == 2);
    NUI_CHECK(state->payload == uri);

    // Hit testing and rejection still apply after focus loss.
    const int offers_before = state->offers;
    auto outside = offer;
    outside.position = {301.0f, 40.0f};
    NUI_CHECK(tree.dispatch(outside, platform) == ui::EventResult::Ignored);
    NUI_CHECK(state->offers == offers_before);
    NUI_CHECK(tree.dispatch(unknown, platform) == ui::EventResult::Handled);
    NUI_CHECK(platform.drop_reject_count == 2);

    // Background drops still obey inherited enabled/visibility state, and
    // changing that state while inactive takes effect without reactivation.
    ui::State<bool> enabled{false};
    ui::State<ui::VisibilityMode> visibility{ui::VisibilityMode::Visible};
    auto gated_state = std::make_shared<DropState>();
    ui::UI gated{ui::Enabled{enabled, ui::Visibility{visibility, DropTarget{gated_state}}}};
    gated.resize({300.0f, 160.0f});
    gated.activate(platform);
    gated.deactivate(platform);
    NUI_CHECK(gated.dispatch(offer, platform) == ui::EventResult::Ignored);
    enabled.set(true);
    NUI_CHECK(gated.dispatch(offer, platform) == ui::EventResult::Handled);
    visibility.set(ui::VisibilityMode::Hidden);
    NUI_CHECK(gated.dispatch(data, platform) == ui::EventResult::Ignored);
    visibility.set(ui::VisibilityMode::Collapsed);
    NUI_CHECK(gated.dispatch(offer, platform) == ui::EventResult::Ignored);
    visibility.set(ui::VisibilityMode::Visible);
    NUI_CHECK(gated.dispatch(data, platform) == ui::EventResult::Handled);
    NUI_CHECK(gated_state->offers == 1 && gated_state->data_events == 1);

    // Mount lifetime, unlike keyboard activation, is a drop-delivery gate.
    auto mounted_state = std::make_shared<DropState>();
    ui::Tree mounted_tree{ui::compile(ui::make_spec(DropTarget{mounted_state}))};
    mounted_tree.layout({300.0f, 160.0f});
    NUI_CHECK(mounted_tree.dispatch(offer, platform) == ui::EventResult::Ignored);
    mounted_tree.mount();
    NUI_CHECK(mounted_tree.dispatch(offer, platform) == ui::EventResult::Handled);
    mounted_tree.unmount();
    NUI_CHECK(mounted_tree.dispatch(data, platform) == ui::EventResult::Ignored);
    NUI_CHECK(mounted_state->offers == 1 && mounted_state->data_events == 0);
}

} // namespace

int main() {
    return test::run("drop", suite);
}
