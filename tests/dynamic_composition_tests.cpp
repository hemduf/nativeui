#include "test_support.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

struct DynamicLog {
    std::vector<std::string> events;
    std::vector<ui::NodeId> mounted_ids;
};

class DynamicProbeComponent final : public ui::Component {
public:
    DynamicProbeComponent(std::string name, std::shared_ptr<DynamicLog> log)
        : name_(std::move(name)), log_(std::move(log)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {80.0f, 30.0f};
    }

    void mount(ui::MountContext& context) override {
        log_->events.push_back(name_ + ".mount");
        log_->mounted_ids.push_back(context.node_id());
    }

    void activate(ui::LifecycleContext&) override {
        log_->events.push_back(name_ + ".activate");
    }

    void deactivate(ui::LifecycleContext&) override {
        log_->events.push_back(name_ + ".deactivate");
    }

    void unmount(ui::LifecycleContext&) override {
        log_->events.push_back(name_ + ".unmount");
    }

    void paint(ui::PaintContext&) const override {}

private:
    std::string name_;
    std::shared_ptr<DynamicLog> log_;
};

class DynamicProbe {
public:
    DynamicProbe(std::string name, std::shared_ptr<DynamicLog> log)
        : name_(std::move(name)), log_(std::move(log)) {}

    ui::Spec spec() && {
        auto name = std::move(name_);
        auto log = std::move(log_);
        return ui::Spec{
            [name = std::move(name), log = std::move(log)]() mutable {
                return std::make_unique<DynamicProbeComponent>(std::move(name), std::move(log));
            },
            {}};
    }

private:
    std::string name_;
    std::shared_ptr<DynamicLog> log_;
};

void suite() {
    ui::State<bool> visible{true};
    auto log = std::make_shared<DynamicLog>();
    test::MockPlatform platform;

    ui::UI tree{ui::If{visible, DynamicProbe{"child", log}}};
    NUI_CHECK((log->events == std::vector<std::string>{"child.mount"}));
    NUI_CHECK(log->mounted_ids.size() == 1);
    const auto first_id = log->mounted_ids.front();

    tree.resize({160.0f, 80.0f});
    tree.activate(platform);
    NUI_CHECK((log->events == std::vector<std::string>{"child.mount", "child.activate"}));

    // State observers only enqueue structural work. The executing state-set
    // stack must never synchronously destroy the retained child.
    visible.set(false);
    NUI_CHECK((log->events == std::vector<std::string>{"child.mount", "child.activate"}));

    // The next structure-dependent top-level checkpoint applies removal in
    // child-safe lifecycle order.
    tree.resize({160.0f, 80.0f});
    NUI_CHECK((log->events == std::vector<std::string>{
        "child.mount", "child.activate", "child.deactivate", "child.unmount"}));

    // Re-insertion is likewise deferred and creates a new retained identity.
    visible.set(true);
    NUI_CHECK(log->mounted_ids.size() == 1);
    tree.resize({160.0f, 80.0f});
    NUI_CHECK(log->mounted_ids.size() == 2);
    NUI_CHECK(log->mounted_ids.back() != first_id);
    NUI_CHECK((log->events == std::vector<std::string>{
        "child.mount", "child.activate", "child.deactivate", "child.unmount",
        "child.mount", "child.activate"}));

    // Equal state is a structural no-op and cannot remount the child.
    visible.set(true);
    tree.resize({160.0f, 80.0f});
    NUI_CHECK(log->mounted_ids.size() == 2);
}

} // namespace

int main() { return test::run("dynamic_composition", &suite); }
