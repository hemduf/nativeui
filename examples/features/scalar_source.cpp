#include "example_support.hpp"

#include <nativeui/noise.hpp>
#include <nativeui/scalar_source.hpp>

#include <utility>

namespace {

int self_test() {
    ui::ScalarSource constant{8.0f};
    auto copied = constant;
    auto moved = std::move(copied);
    moved = std::move(moved);

    const auto noise = ui::NoiseSource::create(
        ui::NoiseType::Value,
        {.feature_size = 24.0f, .seed = 0x5343414cu});
    if (!noise.ok()) {
        return example::fail("ScalarSource noise setup failed");
    }

    auto noise_scalar = ui::ScalarSource::from_noise(noise.noise);
    auto channel_scalar = ui::ScalarSource::from_brush(
        noise.noise.as_brush(
            {0.0f, 0.0f, 0.0f, 1.0f},
            {1.0f, 1.0f, 1.0f, 1.0f}),
        ui::ScalarChannel::Red);
    auto invalid_channel = ui::ScalarSource::from_brush(
        noise.noise.as_brush(
            {0.0f, 0.0f, 0.0f, 1.0f},
            {1.0f, 1.0f, 1.0f, 1.0f}),
        static_cast<ui::ScalarChannel>(255));

    (void)constant;
    (void)moved;
    (void)noise_scalar;
    (void)channel_scalar;
    (void)invalid_channel;
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return self_test();
    return self_test();
}
