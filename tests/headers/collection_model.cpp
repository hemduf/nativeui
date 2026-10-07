#include <nativeui/collection_model.hpp>
void collection_model_standalone_probe() { ui::State<ui::SelectionSnapshot<int>> state{ui::SelectionSnapshot<int>{}}; ui::Selection<int> selection{state}; (void)selection.snapshot(); }
