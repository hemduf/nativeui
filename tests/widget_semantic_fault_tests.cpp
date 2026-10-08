#include "test_support.hpp"
namespace {
struct Runtime { bool fail{}; int destroyed{}; std::function<void()> query; };
class Probe final : public ui::Component {
public:
  explicit Probe(std::shared_ptr<Runtime> runtime) : runtime_(std::move(runtime)) {}
  ~Probe() override { ++runtime_->destroyed; }
  ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {40,20}; }
  void paint(ui::PaintContext&) const override {}
  ui::SemanticInfo semantics() const override {
    auto runtime = runtime_;
    if (std::exchange(runtime->fail,false)) throw std::runtime_error("semantic read");
    if (runtime->query) runtime->query();
    ui::SemanticInfo info; info.role = ui::SemanticRole::Text; info.name = "Owned projection"; return info;
  }
private: std::shared_ptr<Runtime> runtime_;
};
ui::Spec recipe(std::shared_ptr<Runtime> runtime) { return {[runtime] { return std::make_unique<Probe>(runtime); },{}}; }
void throwing_query_preserves_noexcept_and_recovers() {
  auto runtime = std::make_shared<Runtime>(); runtime->fail = true; ui::UI tree{recipe(runtime)};
  NUI_CHECK(!tree.component_semantics(2));
  const auto recovered = tree.component_semantics(2); NUI_CHECK(recovered && recovered->name == "Owned projection");
}
void query_defers_structural_removal_until_next_checkpoint() {
  auto runtime = std::make_shared<Runtime>(); ui::State<bool> present{true};
  ui::UI tree{ui::If{present,recipe(runtime)}}; tree.resize({100,40}); bool called{};
  runtime->query = [&] {
    if (std::exchange(called,true)) return;
    present.set(false); tree.resize({110,40}); NUI_CHECK(runtime->destroyed == 0);
  };
  const auto info = tree.component_semantics(3); NUI_CHECK(info && info->name == "Owned projection");
  NUI_CHECK(called && runtime->destroyed == 0); tree.resize({110,40});
  NUI_CHECK(runtime->destroyed == 1 && !tree.component_semantics(3));
}
void suite() {
  const ui::UI tree{ui::Label{"Const label"}};
  const auto info = tree.component_semantics(2);
  NUI_CHECK(info && info->name == "Const label");
  throwing_query_preserves_noexcept_and_recovers(); query_defers_structural_removal_until_next_checkpoint(); }
}
int main(int argc,char** argv) {
  if (argc == 2 && std::string_view{argv[1]} == "reentrant") return test::run("widget_semantic_reentrant",&query_defers_structural_removal_until_next_checkpoint);
  return test::run("widget_semantic_fault",&suite);
}
