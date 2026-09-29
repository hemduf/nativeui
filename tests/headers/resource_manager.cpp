#include <nativeui/resource_manager.hpp>

#include <type_traits>

static_assert(std::is_copy_constructible_v<ui::ResourceManager>);
static_assert(std::is_copy_assignable_v<ui::ResourceManager>);
static_assert(std::is_move_constructible_v<ui::ResourceManager>);
static_assert(std::is_move_assignable_v<ui::ResourceManager>);
static_assert(std::is_base_of_v<ui::ResourceProvider, ui::ResourceManagerProvider>);

void nativeui_resource_manager_header_compile() {
    const ui::ResourceManager manager{{}};
    (void)manager.valid();
    (void)manager.validation_error();
    (void)manager.resources();
}
