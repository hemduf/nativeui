#include "test_support.hpp"
#include <limits>
namespace {
struct Journal { std::vector<std::size_t> passes; int child_calls{}; bool fail_second{}; };
class Child final : public ui::Component {
public:
  explicit Child(std::shared_ptr<Journal> journal, bool collapsed = false) : journal_(std::move(journal)), collapsed_(collapsed) {}
  ui::ComponentAvailability local_availability() const noexcept override {
    auto result = ui::ComponentAvailability{};
    if (collapsed_) result.visibility = ui::VisibilityMode::Collapsed;
    return result;
  }
  ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
    ++journal_->child_calls;
    if (journal_->fail_second && journal_->passes.back() == 1) {
      journal_->fail_second = false;
      throw std::runtime_error("second measure pass");
    }
    return {60, 20};
  }
  void paint(ui::PaintContext&) const override {}
private: std::shared_ptr<Journal> journal_; bool collapsed_{};
};
class Host final : public ui::Component {
public:
  Host(std::shared_ptr<Journal> journal, std::size_t count) : journal_(std::move(journal)), count_(count) {}
  std::size_t child_measurement_passes() const noexcept override { return count_; }
  ui::Constraints child_constraints(const ui::Constraints& constraints, std::size_t,
                                    const std::vector<ui::ChildMetrics>& metadata) const override {
    NUI_CHECK(metadata.size() == 2 && metadata[0].participates_in_layout && !metadata[1].participates_in_layout);
    auto result = constraints.loosen();
    if (journal_->passes.back() == 1) result.max.w = 30;
    return result;
  }
  ui::Size measure(const std::vector<ui::ChildMetrics>& children) const override {
    NUI_CHECK(children.size() == 2 && !children[1].participates_in_layout);
    return children[0].preferred;
  }
  void paint(ui::PaintContext&) const override {}
private:
  void measure_children_pass_started(const ui::Constraints&, std::size_t pass) const override {
    journal_->passes.push_back(pass);
  }
  std::shared_ptr<Journal> journal_; std::size_t count_{};
};
ui::UI make_tree(std::shared_ptr<Journal> journal, std::size_t count) {
  auto child = ui::Spec{[journal] { return std::make_unique<Child>(journal); }, {}};
  ui::Spec root{[journal,count] { return std::make_unique<Host>(journal,count); },
               {child,ui::Spec{[journal] { return std::make_unique<Child>(journal,true); }, {}}}};
  return ui::UI{std::move(root)};
}
void suite() {
  for (const auto count : {std::size_t{0},std::size_t{1},std::size_t{2},std::numeric_limits<std::size_t>::max()}) {
    auto journal = std::make_shared<Journal>(); auto tree = make_tree(journal,count);
    const auto measured = tree.measure(ui::Constraints::tight({100,40}).loosen());
    const auto expected = count < 2 ? 1 : 2;
    NUI_CHECK(journal->child_calls == expected && journal->passes.size() == static_cast<std::size_t>(expected));
    NUI_CHECK_NEAR(measured.preferred.w,count < 2 ? 60.0f : 30.0f,0.001f);
  }
  auto journal = std::make_shared<Journal>(); journal->fail_second = true; auto tree = make_tree(journal,2);
  bool threw{}; try { (void)tree.measure(ui::Constraints::tight({100,40}).loosen()); } catch (const std::runtime_error&) { threw = true; }
  NUI_CHECK(threw); journal->passes.clear(); journal->child_calls = 0;
  NUI_CHECK_NEAR(tree.measure(ui::Constraints::tight({100,40}).loosen()).preferred.w,30.0f,0.001f);
  NUI_CHECK(journal->passes == std::vector<std::size_t>({0,1}) && journal->child_calls == 2);
}
}
int main() { return test::run("widget_measurement_passes",&suite); }
