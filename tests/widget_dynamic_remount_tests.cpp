#include "test_support.hpp"
#include <nativeui/if.hpp>
#include <nativeui/switch.hpp>
#include <nativeui/for_each.hpp>

#include <map>
#include <memory>
#include <string_view>

namespace {
struct Log {
    std::map<std::string,std::vector<ui::NodeId>> mounts;
    std::map<std::string,int> factories;
    std::map<std::string,int> unmounts;
    std::string fail_mount;
};
class ProbeComponent final : public ui::Component {
public:
    ProbeComponent(std::string key,std::shared_ptr<Log> log) : key_(std::move(key)),log_(std::move(log)) {
        ++log_->factories[key_];
    }
    ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {40.0f,20.0f}; }
    void mount(ui::MountContext& context) override {
        log_->mounts[key_].push_back(context.node_id());
        if (log_->fail_mount == key_) throw std::runtime_error("mount fault");
    }
    void unmount(ui::LifecycleContext&) override { ++log_->unmounts[key_]; }
    void paint(ui::PaintContext&) const override {}
private:
    std::string key_;
    std::shared_ptr<Log> log_;
};
class Probe {
public:
    Probe(std::string key,std::shared_ptr<Log> log) : key_(std::move(key)),log_(std::move(log)) {}
    ui::Spec spec() && {
        const auto key=key_; const auto log=log_;
        return {[key,log] { return std::make_unique<ProbeComponent>(key,log); },{}};
    }
private:
    std::string key_;
    std::shared_ptr<Log> log_;
};
void check_current_remount(const std::shared_ptr<Log>& log,std::string_view key,ui::Tree& tree) {
    const auto name=std::string(key);
    const auto id=log->mounts[name].back();
    const auto constructed=log->factories[name];
    tree.unmount(); tree.mount(); tree.layout({100.0f,80.0f});
    NUI_CHECK(tree.structural_diagnostic().empty());
    NUI_CHECK(log->factories[name] == constructed);
    NUI_CHECK(log->mounts[name].back() == id);
}
void conditional_retained_keys_survive_remount() {
    auto log=std::make_shared<Log>();
    ui::State<bool> present{false};
    ui::Tree tree{ui::compile(ui::make_spec(ui::If{present,Probe{"b",log}}))};
    tree.mount(); tree.layout({100.0f,80.0f});
    present.set(true); tree.layout({100.0f,80.0f});
    NUI_CHECK(log->factories["b"] == 1);
    check_current_remount(log,"b",tree);
    present.set(false); tree.layout({100.0f,80.0f});
    NUI_CHECK_NEAR(tree.measure(ui::Constraints::unbounded()).preferred.h,0.0f,0.001f);
}
void switch_retained_keys_survive_remount() {
    auto log=std::make_shared<Log>();
    ui::State<int> selected{1};
    ui::Tree tree{ui::compile(ui::make_spec(ui::Switch{selected}.when(1,Probe{"a",log}).when(2,Probe{"b",log})))};
    tree.mount(); tree.layout({100.0f,80.0f});
    selected.set(2); tree.layout({100.0f,80.0f});
    NUI_CHECK(log->factories["b"] == 1);
    check_current_remount(log,"b",tree);
}
void foreach_retained_keys_survive_remount() {
    auto log=std::make_shared<Log>();
    ui::State<std::vector<std::string>> items{{"a","b"}};
    auto make=[log](const std::string& key) { return Probe{key,log}; };
    ui::Tree tree{ui::compile(ui::make_spec(ui::ForEach{items,[](const std::string& key) { return key; },make}))};
    tree.mount(); tree.layout({100.0f,80.0f});
    items.set({"c"}); tree.layout({100.0f,80.0f});
    NUI_CHECK(log->factories["c"] == 1);
    check_current_remount(log,"c",tree);
}
void source_changed_while_unmounted_reconciles_from_retained_children() {
    auto log=std::make_shared<Log>();
    ui::State<std::vector<std::string>> items{{"a","b"}};
    ui::Tree tree{ui::compile(ui::make_spec(ui::ForEach{items,[](const std::string& key) { return key; },
        [log](const std::string& key) { return Probe{key,log}; }}))};
    tree.mount(); tree.layout({100.0f,80.0f});
    items.set({"c"}); tree.layout({100.0f,80.0f});
    tree.unmount(); items.set({"d"}); tree.mount(); tree.layout({100.0f,80.0f});
    NUI_CHECK(tree.structural_diagnostic().empty());
    NUI_CHECK(log->factories["d"] == 1 && log->unmounts["c"] == 2);
}
void failed_remount_keeps_retained_identity_for_retry() {
    auto log=std::make_shared<Log>();
    ui::State<int> selected{1};
    ui::Tree tree{ui::compile(ui::make_spec(ui::Switch{selected}.when(1,Probe{"a",log}).when(2,Probe{"b",log})))};
    tree.mount(); tree.layout({100.0f,80.0f});
    selected.set(2); tree.layout({100.0f,80.0f});
    const auto id=log->mounts["b"].back();
    tree.unmount(); log->fail_mount="b";
    bool threw=false;
    try { tree.mount(); } catch (const std::runtime_error&) { threw=true; }
    NUI_CHECK(threw);
    log->fail_mount.clear(); tree.mount(); tree.layout({100.0f,80.0f});
    NUI_CHECK(tree.structural_diagnostic().empty());
    NUI_CHECK(log->factories["b"] == 1 && log->mounts["b"].back() == id);
}
void suite() {
    conditional_retained_keys_survive_remount();
    switch_retained_keys_survive_remount();
    foreach_retained_keys_survive_remount();
    source_changed_while_unmounted_reconciles_from_retained_children();
    failed_remount_keeps_retained_identity_for_retry();
}
}
int main(int argc,char** argv) {
    const std::string_view mode=argc>1 ? argv[1] : "all";
    if (mode=="if") return test::run("dynamic_remount_if",&conditional_retained_keys_survive_remount);
    if (mode=="switch") return test::run("dynamic_remount_switch",&switch_retained_keys_survive_remount);
    if (mode=="foreach") return test::run("dynamic_remount_foreach",&foreach_retained_keys_survive_remount);
    if (mode=="unmounted") return test::run("dynamic_remount_changed",&source_changed_while_unmounted_reconciles_from_retained_children);
    if (mode=="retry") return test::run("dynamic_remount_retry",&failed_remount_keeps_retained_identity_for_retry);
    return test::run("widget_dynamic_remount",&suite);
}
