#include "test_support.hpp"
#include "../src/detail/view_geometry.hpp"

#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

struct LifecycleLog {
    std::vector<std::string> events;
    std::map<std::string, ui::NodeId> ids;
    int observed_changes{};
};

class LifecycleProbeComponent final : public ui::Component {
public:
    LifecycleProbeComponent(
        std::string name, std::shared_ptr<LifecycleLog> log, ui::State<int>& observed)
        : name_(std::move(name)), log_(std::move(log)), observed_(observed) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {80.0f, 30.0f};
    }

    void mount(ui::MountContext& context) override {
        record("mount", context.node_id());
        subscription_ = observed_.observe([log = log_](const int&) { ++log->observed_changes; });
    }

    void activate(ui::LifecycleContext& context) override {
        record("activate", context.node_id());
    }

    void deactivate(ui::LifecycleContext& context) override {
        record("deactivate", context.node_id());
    }

    void unmount(ui::LifecycleContext& context) override {
        record("unmount", context.node_id());
    }

    ui::EventResult input(const ui::InputEvent&, ui::InputContext&) override {
        return ui::EventResult::Handled;
    }

    void paint(ui::PaintContext&) const override {}

private:
    void record(std::string_view phase, ui::NodeId id) {
        const auto [it, inserted] = log_->ids.emplace(name_, id);
        NUI_CHECK(inserted || it->second == id);
        log_->events.push_back(name_ + "." + std::string(phase));
    }

    std::string name_;
    std::shared_ptr<LifecycleLog> log_;
    ui::State<int>& observed_;
    ui::State<int>::Subscription subscription_;
};

class LifecycleProbe {
public:
    LifecycleProbe(std::string name, std::shared_ptr<LifecycleLog> log, ui::State<int>& observed)
        : name_(std::move(name)), log_(std::move(log)), observed_(&observed) {}

    ui::Spec spec() && {
        auto name = std::move(name_);
        auto log = std::move(log_);
        auto* observed = observed_;
        return ui::Spec{
            [name = std::move(name), log = std::move(log), observed]() mutable {
                return std::make_unique<LifecycleProbeComponent>(
                    std::move(name), std::move(log), *observed);
            },
            {}};
    }

private:
    std::string name_;
    std::shared_ptr<LifecycleLog> log_;
    ui::State<int>* observed_{};
};

void check_size(ui::Size actual, ui::Size expected) {
    NUI_CHECK_NEAR(actual.w, expected.w, 0.00001f);
    NUI_CHECK_NEAR(actual.h, expected.h, 0.00001f);
}

void t043_geometry_contract() {
    ui::detail::ViewGeometryState geometry{{160.0f, 90.0f}};
    NUI_CHECK_NEAR(geometry.last_valid_scale(), 1.0f, 0.00001f);

    for (const float scale : {1.0f, 1.25f, 1.5f, 2.0f}) {
        const auto configured = geometry.configure({160.0f * scale, 90.0f * scale}, scale);
        NUI_CHECK(configured.has_value());
        check_size(*configured, {160.0f, 90.0f});
        NUI_CHECK_NEAR(geometry.last_valid_scale(), scale, 0.00001f);
    }

    NUI_CHECK(geometry.configure({240.0f, 135.0f}, 1.5f).has_value());
    for (const float invalid : {0.0f,
                                -1.0f,
                                std::numeric_limits<float>::infinity(),
                                std::numeric_limits<float>::quiet_NaN()}) {
        const auto configured = geometry.configure({300.0f, 150.0f}, invalid);
        NUI_CHECK(configured.has_value());
        check_size(*configured, {200.0f, 100.0f});
        NUI_CHECK_NEAR(geometry.last_valid_scale(), 1.5f, 0.00001f);
    }

    const auto last_logical = geometry.logical_size();
    NUI_CHECK(!geometry.configure({0.0f, 150.0f}, 2.0f).has_value());
    NUI_CHECK(!geometry.renderable());
    check_size(geometry.logical_size(), last_logical);
    NUI_CHECK_NEAR(geometry.last_valid_scale(), 2.0f, 0.00001f);

    NUI_CHECK(!geometry.physical_request({0.0f, 20.0f}).has_value());
    NUI_CHECK(!geometry.physical_request({std::numeric_limits<float>::quiet_NaN(), 20.0f}).has_value());
    const auto request = geometry.physical_request({100.25f, 50.25f});
    NUI_CHECK(request.has_value());
    check_size(*request, {201.0f, 101.0f});
    NUI_CHECK(!geometry.pending_request().has_value());
    geometry.record_successful_request({100.25f, 50.25f});
    NUI_CHECK(geometry.pending_request().has_value());

    const auto physical = ui::detail::logical_to_physical_covering_rect(
        {0.2f, 1.2f, 10.2f, 4.2f}, 1.5f);
    NUI_CHECK_NEAR(physical.x, 0.0f, 0.00001f);
    NUI_CHECK_NEAR(physical.y, 1.0f, 0.00001f);
    NUI_CHECK_NEAR(physical.w, 16.0f, 0.00001f);
    NUI_CHECK_NEAR(physical.h, 8.0f, 0.00001f);

    const auto point = ui::detail::physical_to_logical_point({18.75f, 11.25f}, 1.25f);
    NUI_CHECK_NEAR(point.x, 15.0f, 0.00001f);
    NUI_CHECK_NEAR(point.y, 9.0f, 0.00001f);

    ui::detail::ViewGeometryState other{{100.0f, 50.0f}};
    NUI_CHECK(other.configure({200.0f, 100.0f}, 2.0f).has_value());
    NUI_CHECK_NEAR(other.last_valid_scale(), 2.0f, 0.00001f);
    NUI_CHECK_NEAR(geometry.last_valid_scale(), 2.0f, 0.00001f);

    ui::detail::PreferredSizeState preferred;
    std::vector<ui::Size> notifications;
    preferred.queue({100.0f, 50.0f});
    NUI_CHECK(preferred.dispatch_once([&](ui::Size size) {
        notifications.push_back(size);
        preferred.queue({120.0f, 60.0f});
        NUI_CHECK(!preferred.dispatch_once([&](ui::Size) { NUI_CHECK(false); }));
    }));
    NUI_CHECK(notifications.size() == 1);
    NUI_CHECK(preferred.dispatch_once([&](ui::Size size) { notifications.push_back(size); }));
    NUI_CHECK(notifications.size() == 2);

    preferred.queue({120.0001f, 60.0f});
    NUI_CHECK(!preferred.dispatch_once([&](ui::Size) { NUI_CHECK(false); }));
    preferred.queue({120.00011f, 60.0f});
    NUI_CHECK(preferred.dispatch_once([&](ui::Size size) { notifications.push_back(size); }));

    preferred.queue({130.0f, 70.0f});
    preferred.queue({140.0f, 80.0f});
    NUI_CHECK(preferred.dispatch_once([&](ui::Size size) { notifications.push_back(size); }));
    check_size(notifications.back(), {140.0f, 80.0f});
}

void suite() {
    t043_geometry_contract();

    // Existing focus/resize lifecycle regression.
    {
        auto probe = std::make_shared<test::ProbeState>();
        ui::UI tree{ui::Padding{10.0f, test::Probe{probe}}};
        test::MockPlatform platform;

        tree.resize({120.0f, 80.0f});
        NUI_CHECK(tree.dirty());

        tree.activate(platform);
        NUI_CHECK(probe->focus_in == 1);
        NUI_CHECK(probe->focus_out == 0);
        NUI_CHECK(probe->focus_bounds.size() == 1);
        NUI_CHECK_NEAR(probe->focus_bounds.back().x, 10.0f, 0.0001f);
        NUI_CHECK_NEAR(probe->focus_bounds.back().y, 10.0f, 0.0001f);
        NUI_CHECK_NEAR(probe->focus_bounds.back().w, 100.0f, 0.0001f);
        NUI_CHECK_NEAR(probe->focus_bounds.back().h, 60.0f, 0.0001f);

        tree.activate(platform);
        NUI_CHECK(probe->focus_in == 1);

        tree.resize({220.0f, 120.0f});
        tree.refresh_focus(platform);
        NUI_CHECK(probe->focus_in == 2);
        NUI_CHECK_NEAR(probe->focus_bounds.back().w, 200.0f, 0.0001f);
        NUI_CHECK_NEAR(probe->focus_bounds.back().h, 100.0f, 0.0001f);

        tree.dispatch(test::key(ui::Key::Right), platform);
        NUI_CHECK(probe->key_events == 1);

        tree.deactivate(platform);
        NUI_CHECK(probe->focus_out == 1);
        tree.deactivate(platform);
        NUI_CHECK(probe->focus_out == 1);
        NUI_CHECK(tree.dispatch(test::key(ui::Key::Right), platform) == ui::EventResult::Ignored);
        NUI_CHECK(probe->key_events == 1);

        tree.activate(platform);
        NUI_CHECK(probe->focus_in == 3);
    }

    // Text-input platform service follows focus activation/deactivation.
    {
        test::MockPlatform platform;
        ui::State<std::string> text{"Init"};
        ui::UI text_tree{ui::TextInput{"Name", text}};
        text_tree.resize({320.0f, 90.0f});
        text_tree.activate(platform);
        NUI_CHECK(platform.text_input_active);
        text_tree.deactivate(platform);
        NUI_CHECK(!platform.text_input_active);
    }

    // Node IDs are unique within a compiled tree before any platform exists.
    {
        auto root = ui::compile(ui::make_spec(ui::Column{ui::Spacer{1.0f}, ui::Spacer{2.0f}}));
        NUI_CHECK(root->id != ui::kInvalidNodeId);
        NUI_CHECK(root->children.size() == 2);
        NUI_CHECK(root->children[0]->id != root->id);
        NUI_CHECK(root->children[1]->id != root->id);
        NUI_CHECK(root->children[0]->id != root->children[1]->id);
    }

    // Deterministic mount/activate/deactivate/unmount order and stable IDs.
    ui::State<int> observed{0};
    auto log = std::make_shared<LifecycleLog>();
    {
        test::MockPlatform platform;
        ui::UI tree{
            ui::Column{
                LifecycleProbe{"A", log, observed},
                LifecycleProbe{"B", log, observed},
            }.padding(0.0f).gap(0.0f)
        };

        NUI_CHECK((log->events == std::vector<std::string>{"A.mount", "B.mount"}));
        NUI_CHECK(log->ids.at("A") != log->ids.at("B"));

        tree.resize({200.0f, 100.0f});
        tree.activate(platform);
        NUI_CHECK((log->events == std::vector<std::string>{
            "A.mount", "B.mount", "A.activate", "B.activate"}));

        observed.set(1);
        NUI_CHECK(log->observed_changes == 2);

        tree.deactivate(platform);
        NUI_CHECK((log->events == std::vector<std::string>{
            "A.mount", "B.mount", "A.activate", "B.activate",
            "B.deactivate", "A.deactivate"}));
    }

    NUI_CHECK((log->events == std::vector<std::string>{
        "A.mount", "B.mount", "A.activate", "B.activate",
        "B.deactivate", "A.deactivate", "B.unmount", "A.unmount"}));

    // Component destruction releases State subscriptions; later changes cannot
    // call into removed runtime nodes.
    observed.set(2);
    NUI_CHECK(log->observed_changes == 2);
}

} // namespace

int main() { return test::run("lifecycle", &suite); }
