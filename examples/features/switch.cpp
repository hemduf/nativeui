#include "example_support.hpp"
#include <nativeui/switch.hpp>

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
    ui::State<int> value{1};
    ui::UI tree{ui::Switch{value}.when(1,ui::Spacer{80.0f,40.0f})
        .when(2,ui::Spacer{100.0f,50.0f}).otherwise(ui::Spacer{20.0f,10.0f})};
    tree.resize({200.0f,100.0f});
    if (tree.measure().preferred.w != 80.0f) return example::fail("Switch first branch mismatch");
    value.set(2); tree.resize({200.0f,100.0f});
    if (tree.measure().preferred.w != 100.0f) return example::fail("Switch second branch mismatch");
    value.set(99); tree.resize({200.0f,100.0f});
    if (tree.measure().preferred.w != 20.0f) return example::fail("Switch fallback mismatch");
    auto first_log=std::make_shared<RetainedLeafLog>(),second_log=std::make_shared<RetainedLeafLog>();
    ui::State<int> retained{1};
    ui::Tree compiled{ui::compile(ui::make_spec(ui::Switch{retained}.when(1,RetainedLeaf{first_log}).when(2,RetainedLeaf{second_log})))};
    compiled.mount(); retained.set(2); compiled.layout({200.0f,100.0f});
    const auto identity=second_log->mounts.back();
    compiled.unmount(); compiled.mount(); compiled.layout({200.0f,100.0f});
    if (second_log->constructions!=1 || second_log->mounts.back()!=identity || !compiled.structural_diagnostic().empty())
        return example::fail("Switch remount rebuilt or relabeled the retained branch");
    auto recipe=ui::make_spec(ui::Switch{retained}.when(2,RetainedLeaf{second_log}));
    ui::UI first{ui::Spec{recipe}},second{ui::Spec{recipe}};
    if (second_log->constructions!=3) return example::fail("copied Switch recipes shared a component instance");
    ui::HeadlessRenderer renderer{{200.0f,100.0f},1.0f}; return renderer.render(tree) ? 0 : example::fail("Switch headless render failed");
}
}
int main(int argc,char** argv) {
    if (example::self_test_requested(argc,argv)) return self_test();
    ui::State<int> page{1};
    ui::UI tree{ui::Column{ui::Header{"Switch composition"},ui::Row{
        ui::Button{"First",[&] { page.set(1); }},ui::Button{"Second",[&] { page.set(2); }}},
        ui::Switch{page}.when(1,ui::Label{"First retained branch"}).when(2,ui::Label{"Second retained branch"})}.padding(16.0f)};
    return example::run_window(tree,"NativeUI Switch",{480.0f,220.0f});
}
