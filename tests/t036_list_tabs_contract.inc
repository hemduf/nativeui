#pragma once

#include <nativeui/nativeui.hpp>

#include <optional>
#include <utility>

namespace t036_contract {

inline void public_builder_contract() {
    ui::State<std::optional<int>> list_selection{std::nullopt};
    ui::State<int> tab_selection{1};

    auto list = ui::ListView<int>{list_selection}
                    .item(1, ui::Label{"One"})
                    .item(2, ui::Label{"Two"}, false);
    auto tabs = ui::Tabs<int>{tab_selection}
                    .tab(1, "One", ui::Label{"Panel one"})
                    .tab(2, "Two", ui::Label{"Panel two"}, false);

    [[maybe_unused]] auto list_spec = std::move(list).spec();
    [[maybe_unused]] auto tabs_spec = std::move(tabs).spec();
}

} // namespace t036_contract
