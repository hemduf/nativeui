#include "example_support.hpp"
#include <nativeui/for_each.hpp>
#include <algorithm>

namespace {
struct RetainedLeafLog {
    int constructions{};
    std::vector<ui::NodeId> mounts;
};
class RetainedLeafComponent final : public ui::Component {
public:
    explicit RetainedLeafComponent(std::shared_ptr<RetainedLeafLog> log) : log_(std::move(log)) { ++log_->constructions; }
    ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {80.0f,40.0f}; }
    void mount(ui::MountContext& context) override { log_->mounts.push_back(context.node_id()); }
    void paint(ui::PaintContext&) const override {}
private:
    std::shared_ptr<RetainedLeafLog> log_;
};
class RetainedLeaf {
public:
    explicit RetainedLeaf(std::shared_ptr<RetainedLeafLog> log) : log_(std::move(log)) {}
    ui::Spec spec() && { const auto log=log_; return {[log] { return std::make_unique<RetainedLeafComponent>(log); },{}}; }
private:
    std::shared_ptr<RetainedLeafLog> log_;
};
int self_test() {
    ui::State<std::vector<int>> values{{1,2,3}};
    int recipes=0; auto log=std::make_shared<RetainedLeafLog>();
    auto builder=ui::ForEach{values,[](int value) { return value; },[&](int) { ++recipes; return RetainedLeaf{log}; }};
    ui::Tree tree{ui::compile(ui::make_spec(std::move(builder)))};
    tree.mount(); tree.layout({200.0f,100.0f});
    if (recipes!=3 || log->constructions!=3) return example::fail("ForEach initial children mismatch");
    values.set({3,1,2}); tree.layout({200.0f,100.0f});
    if (log->constructions!=3) return example::fail("ForEach reorder reconstructed retained components");
    values.set({1,4}); tree.layout({200.0f,100.0f});
    if (log->constructions!=4) return example::fail("ForEach did not construct only the inserted key");
    const auto recipe_count=recipes,component_count=log->constructions;
    const auto identity=log->mounts.back();
    tree.unmount(); tree.mount(); tree.layout({200.0f,100.0f});
    if (recipes!=recipe_count || log->constructions!=component_count || log->mounts.back()!=identity || !tree.structural_diagnostic().empty())
        return example::fail("ForEach remount repeated factories or changed retained identity");
    values.set({}); tree.layout({200.0f,100.0f});
    if (tree.measure(ui::Constraints::unbounded()).preferred.w!=0.0f) return example::fail("ForEach empty snapshot retained children");
    ui::State<std::vector<int>> copied_values{{7}}; int copied_recipes=0;
    auto copied=ui::make_spec(ui::ForEach{copied_values,[](int value) { return value; },[&](int) { ++copied_recipes; return RetainedLeaf{log}; }});
    ui::UI first{ui::Spec{copied}},second{ui::Spec{copied}};
    if (copied_recipes!=2 || log->constructions!=component_count+2) return example::fail("copied ForEach recipes shared children");
    ui::HeadlessRenderer renderer{{200.0f,100.0f},1.0f};
    return renderer.render(first) && renderer.render(second)?0:example::fail("ForEach headless render failed");
}
}
int main(int argc,char** argv) {
    if (example::self_test_requested(argc,argv)) return self_test();
    ui::State<std::vector<std::string>> values{{"First","Second","Third"}};
    ui::UI tree{ui::Column{ui::Header{"ForEach retained keys"},
        ui::ForEach{values,[](const std::string& value) { return value; },[](const std::string& value) {
            const float top = value == "First" ? 0.0f : value == "Second" ? 28.0f : 56.0f;
            return ui::Column{ui::Spacer{0.0f,top},ui::Label{value}}.padding(0.0f).gap(0.0f);
        }},ui::Button{"Reverse logical order",[&] { auto next = values.get(); std::reverse(next.begin(),next.end()); values.set(std::move(next)); }}}.padding(16.0f)};
    return example::run_window(tree,"NativeUI ForEach",{440.0f,200.0f});
}
