#include "example_support.hpp"
#include <nativeui/if.hpp>

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
    ui::State<bool> shown{false};
    ui::UI tree{ui::If{shown,ui::Spacer{80.0f,40.0f}}};
    if (tree.measure().preferred.w != 0.0f) return example::fail("If false retained a child");
    shown.set(true); tree.resize({200.0f,100.0f});
    if (tree.measure().preferred.w != 80.0f) return example::fail("If true did not mount child");
    shown.set(false); tree.resize({200.0f,100.0f});
    if (tree.measure().preferred.h != 0.0f) return example::fail("If false did not unmount child");
    auto log=std::make_shared<RetainedLeafLog>();
    ui::State<bool> retained{false};
    ui::Tree compiled{ui::compile(ui::make_spec(ui::If{retained,RetainedLeaf{log}}))};
    compiled.mount(); retained.set(true); compiled.layout({200.0f,100.0f});
    const auto identity=log->mounts.back();
    compiled.unmount(); compiled.mount(); compiled.layout({200.0f,100.0f});
    if (log->constructions!=1 || log->mounts.back()!=identity || !compiled.structural_diagnostic().empty())
        return example::fail("If remount rebuilt or relabeled the retained child");
    auto recipe=ui::make_spec(ui::If{retained,RetainedLeaf{log}});
    ui::UI first{ui::Spec{recipe}},second{ui::Spec{recipe}};
    if (log->constructions!=3) return example::fail("copied If recipes did not create independent component instances");
    ui::HeadlessRenderer renderer{{200.0f,100.0f},1.0f}; return renderer.render(tree) ? 0 : example::fail("If headless render failed");
}
}
int main(int argc,char** argv) {
    if (example::self_test_requested(argc,argv)) return self_test();
    ui::State<bool> shown{true};
    ui::UI tree{ui::Column{ui::Header{"Conditional composition"},ui::Checkbox{shown,"Show details"},
        ui::If{shown,ui::Label{"This subtree is mounted only while the application state is true."}}}.padding(16.0f)};
    return example::run_window(tree,"NativeUI If",{600.0f,220.0f});
}
