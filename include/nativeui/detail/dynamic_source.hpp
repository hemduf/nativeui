#pragma once

#include <nativeui/component_base.hpp>

#include <functional>
#include <string>
#include <vector>

namespace ui::detail {

struct DynamicChildSpec {
    std::string key;
    Spec spec;
};

class DynamicChildrenSource {
public:
    virtual ~DynamicChildrenSource() = default;

    [[nodiscard]] virtual std::vector<std::string> desired_keys() const = 0;
    [[nodiscard]] virtual std::vector<DynamicChildSpec> desired_children() const = 0;
    virtual void set_structure_invalidator(std::function<void()> invalidator) = 0;
};

/// Optional retained-tree pointer-routing policy for structural wrappers.
/// Implementations may suppress pointer hit testing for their entire child
/// subtree without changing layout, painting, availability or keyboard focus.
/// The default tree path is unchanged for components that do not implement it.
class PointerDescendantPolicy {
public:
    virtual ~PointerDescendantPolicy() = default;
    [[nodiscard]] virtual bool pointer_descendants_targetable() const noexcept = 0;
};

} // namespace ui::detail
