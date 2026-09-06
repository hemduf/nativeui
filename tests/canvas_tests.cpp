#include "test_support.hpp"

namespace {

void suite() {
    test::MockPlatform platform;
    int down_count = 0;
    int move_count = 0;
    int up_count = 0;
    int key_count = 0;
    ui::Point down_local{};
    ui::Point captured_move_local{};

    ui::UI tree{
        ui::Padding{20.0f,
            ui::Canvas{
                ui::Size{200.0f, 100.0f},
                [](ui::CanvasContext2D&) {}}
                .on_input([&](const ui::InputEvent& event, ui::CanvasInputContext& ctx) {
                    switch (event.type) {
                        case ui::InputType::PointerDown:
                            ++down_count;
                            down_local = event.position;
                            ctx.capture_pointer();
                            break;
                        case ui::InputType::PointerMove:
                            ++move_count;
                            captured_move_local = event.position;
                            break;
                        case ui::InputType::PointerUp:
                            ++up_count;
                            ctx.release_pointer();
                            break;
                        case ui::InputType::KeyDown:
                            if (event.key == ui::Key::Right) ++key_count;
                            break;
                        default:
                            break;
                    }
                })
        }
    };

    tree.resize({240.0f, 140.0f});
    tree.activate(platform);

    tree.dispatch(test::pointer(ui::InputType::PointerDown, 30.0f, 45.0f), platform);
    NUI_CHECK(down_count == 1);
    NUI_CHECK_NEAR(down_local.x, 10.0f, 0.0001f);
    NUI_CHECK_NEAR(down_local.y, 25.0f, 0.0001f);

    // Pointer capture keeps routing to the canvas even far outside its bounds.
    tree.dispatch(test::pointer(ui::InputType::PointerMove, 500.0f, 500.0f), platform);
    NUI_CHECK(move_count == 1);
    NUI_CHECK_NEAR(captured_move_local.x, 480.0f, 0.0001f);
    NUI_CHECK_NEAR(captured_move_local.y, 480.0f, 0.0001f);

    tree.dispatch(test::pointer(ui::InputType::PointerUp, 500.0f, 500.0f), platform);
    NUI_CHECK(up_count == 1);

    // Release ends capture: another outside motion is no longer delivered.
    tree.dispatch(test::pointer(ui::InputType::PointerMove, 500.0f, 500.0f), platform);
    NUI_CHECK(move_count == 1);

    // PointerDown focused the interactive Canvas; keyboard now routes to it.
    tree.dispatch(test::key(ui::Key::Right), platform);
    NUI_CHECK(key_count == 1);

    // Deactivation must also clear toolkit-level capture.
    tree.dispatch(test::pointer(ui::InputType::PointerDown, 30.0f, 45.0f), platform);
    NUI_CHECK(down_count == 2);
    tree.deactivate(platform);
    tree.dispatch(test::pointer(ui::InputType::PointerMove, 500.0f, 500.0f), platform);
    NUI_CHECK(move_count == 1);
}

} // namespace

int main() { return test::run("canvas", &suite); }
