#include "example_support.hpp"

#include <nativeui/material.hpp>

#include <iostream>
#include <utility>

namespace {

ui::Material make_panel_material() {
    ui::Material material{
        ui::Brush{ui::Color{0.65f, 0.67f, 0.70f, 1.0f}}};
    material.set_metallic(1.0f)
            .set_roughness(0.22f);
    return material;
}

int self_test() {
    auto material = make_panel_material();
    if (material.has_emissive()) {
        return example::fail("Material unexpectedly starts emissive");
    }

    material.set_emissive(
        ui::Brush{ui::Color{1.0f, 0.35f, 0.10f, 1.0f}},
        8.0f);
    if (!material.has_emissive()) {
        return example::fail("Material did not enable emissive state");
    }

    ui::Material copy{material};
    material.clear_emissive();
    if (material.has_emissive() || !copy.has_emissive()) {
        return example::fail("Material copy ownership was not independent");
    }

    ui::Material moved{std::move(copy)};
    if (!moved.has_emissive() || copy.has_emissive()) {
        return example::fail("Material move state was not canonical");
    }

    moved.clear_emissive();
    return moved.has_emissive()
        ? example::fail("Material clear_emissive did not disable emissive state")
        : 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();

    auto material = make_panel_material();
    material.set_emissive(
        ui::Brush{ui::Color{1.0f, 0.35f, 0.10f, 1.0f}},
        8.0f);

    std::cout
        << "NativeUI Material value description prepared: "
        << (material.has_emissive() ? "emissive enabled" : "emissive disabled")
        << '\n';
    return 0;
}
