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
}

} // namespace

int main() {
    return test::run("drop", suite);
}
