#include <nativeui/grid_view.hpp>
namespace { struct ProbeKey { int value; bool operator==(const ProbeKey&) const = default; }; }
void grid_view_standalone_probe() {
  ui::State<std::vector<ui::CollectionItem<ProbeKey>>> rows{std::vector<ui::CollectionItem<ProbeKey>>{}};
  ui::State<ui::SelectionSnapshot<ProbeKey>> selected{ui::SelectionSnapshot<ProbeKey>{}};
  ui::Selection<ProbeKey> selection{selected};
  auto recipe = ui::GridView<ProbeKey>{rows, selection}.spec();
  (void)recipe;
}
