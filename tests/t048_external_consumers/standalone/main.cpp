#include <nativeui/nativeui.hpp>

#include <string_view>

int main(int argc, char** argv) {
    if (argc != 2 || std::string_view{argv[1]} != "--self-test") return 2;

    ui::State<bool> enabled{true};
    ui::UI ui_tree{
        ui::Column{
            ui::Header{"T048 standalone consumer"},
            ui::Toggle{"Enabled", enabled},
        }.padding(8.0f).gap(4.0f)};

    ui::StandaloneWindow window{
        ui_tree,
        ui::WindowDesc{.title = "T048 standalone consumer",
                       .size = {240.0f, 120.0f},
                       .resizable = true}};
    if (!window.native_handle() || !(window.scale_factor() > 0.0f)) return 3;

    for (int i = 0; i < 4; ++i) (void)window.poll(0.0);
    if (!window.last_error().empty()) return 4;

    window.request_close();
    return window.should_close() ? 0 : 5;
}
