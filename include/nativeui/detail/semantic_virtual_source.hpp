#pragma once

#include <nativeui/semantics.hpp>

namespace ui::detail {

/// Internal capability implemented only by retained components that own a
/// specialized virtual semantic collection. Ordinary custom components expose
/// semantics exclusively through Component::semantics().
class VirtualSemanticChildrenSource {
public:
    virtual ~VirtualSemanticChildrenSource() = default;

    [[nodiscard]] virtual VirtualSemanticChildren virtual_semantic_children(
        Rect semantic_bounds) const = 0;
};

} // namespace ui::detail
