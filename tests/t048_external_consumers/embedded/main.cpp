#include <nativeui/nativeui.hpp>

#include <chrono>
#include <string_view>

int main(int argc, char** argv) {
    if (argc != 2 || std::string_view{argv[1]} != "--self-test") return 2;

    ui::UI parent_ui{
        ui::Column{
            ui::Header{"T048 embedded host seam"},
            ui::Spacer{80.0f},
        }.padding(8.0f).gap(4.0f)};
    ui::StandaloneWindow parent{
        parent_ui,
        ui::WindowDesc{.title = "T048 embedded host seam",
                       .size = {300.0f, 180.0f},
                       .resizable = true}};
    if (!parent.native_handle()) return 3;

    ui::State<bool> enabled{true};
    ui::UI child_ui{
        ui::Column{
            ui::Header{"T048 embedded consumer"},
            ui::Toggle{"Enabled", enabled},
        }.padding(8.0f).gap(4.0f)};
    ui::EmbeddedView child{child_ui, parent.native_handle(), {220.0f, 100.0f}};
    if (!child.native_handle() || !(child.scale_factor() > 0.0f)) return 4;

    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 64; ++i) {
        (void)parent.poll(0.0);
        (void)child.poll();
    }
    if (std::chrono::steady_clock::now() - start > std::chrono::seconds(1)) return 5;
    if (!parent.last_error().empty() || !child.last_error().empty()) return 6;

    if (!child.set_size({230.0f, 110.0f})) return 7;
    child.request_close();
    if (!child.should_close()) return 8;
    parent.request_close();
    return parent.should_close() ? 0 : 9;
}
