#pragma once

#include <nativeui/semantics.hpp>

#include <optional>

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

/// Resolve a virtual semantic item through the immutable token index used by
/// T067/T068 live action routing.
///
/// Native accessibility actions must never degrade into a linear scan of a
/// potentially 100k-item logical collection. Sources that do not publish an
/// indexed snapshot therefore fail closed on the live action path; read-only
/// snapshot APIs may still use VirtualSemanticChildren::item_for_token() when a
/// linear fallback is explicitly acceptable outside action dispatch.
[[nodiscard]] inline std::optional<VirtualSemanticItem>
resolve_indexed_virtual_semantic_item(
    const VirtualSemanticChildren& children,
    VirtualSemanticItemToken token) {
    if (token == kInvalidVirtualSemanticItemToken) {
        return std::nullopt;
    }

    const auto& metadata = children.metadata_snapshot();
    const auto& token_index = children.token_index_snapshot();
    if (!metadata || !token_index) {
        return std::nullopt;
    }

    const auto found = token_index->find(token);
    if (found == token_index->end() || found->second >= metadata->size() ||
        (*metadata)[found->second].token != token) {
        return std::nullopt;
    }

    return children.item_at(found->second);
}

} // namespace ui::detail
