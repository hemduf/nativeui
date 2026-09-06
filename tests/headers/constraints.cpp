#include <nativeui/constraints.hpp>

void nativeui_header_constraints_compile() {
    const auto c = ui::Constraints::loose({100.0f, 80.0f});
    (void)c.constrain({120.0f, 20.0f});
}
