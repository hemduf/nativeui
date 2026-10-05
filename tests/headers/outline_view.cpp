#include <nativeui/outline_view.hpp>
namespace { struct ProbeKey { int value; bool operator==(const ProbeKey&) const = default; }; }
void outline_view_standalone_probe() {
  ui::State<std::vector<ui::TreeNode<ProbeKey>>> rows{std::vector<ui::TreeNode<ProbeKey>>{}};
  ui::State<ui::SelectionSnapshot<ProbeKey>> selected{ui::SelectionSnapshot<ProbeKey>{}};
  ui::Selection<ProbeKey> selection{selected};
  ui::State<std::vector<ProbeKey>> expanded{std::vector<ProbeKey>{}};
  auto recipe = ui::OutlineView<ProbeKey>{rows, selection, expanded}.spec();
  (void)recipe;
}
