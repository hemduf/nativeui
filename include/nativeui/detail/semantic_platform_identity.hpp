#pragma once

#include <nativeui/semantics.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace ui::detail {

/// Immutable per-view accessibility namespace identity.
///
/// The application instance and view IDs are minted by their owning runtime
/// layer and must be non-zero.  This value object deliberately contains no
/// pointer-derived or process-global state, so destroying and recreating a
/// native view cannot make a stale platform path silently resolve to an
/// unrelated root when the owner supplies a fresh view ID.
class AccessibilityRootIdentity final {
public:
    using InstanceId = std::uint64_t;
    using ViewId = std::uint64_t;

    [[nodiscard]] static std::optional<AccessibilityRootIdentity> create(
        InstanceId application_instance_id,
        ViewId view_id) noexcept {
        if (application_instance_id == 0 || view_id == 0) {
            return std::nullopt;
        }
        return AccessibilityRootIdentity{application_instance_id, view_id};
    }

    [[nodiscard]] InstanceId application_instance_id() const noexcept {
        return application_instance_id_;
    }

    [[nodiscard]] ViewId view_id() const noexcept {
        return view_id_;
    }

    [[nodiscard]] std::string atspi_node_path(SemanticId node_id) const {
        if (node_id == kInvalidSemanticId) {
            return {};
        }
        return root_path() + "/" + std::to_string(node_id);
    }

    [[nodiscard]] std::string atspi_virtual_item_path(
        SemanticId list_node_id,
        VirtualSemanticItemToken item_token) const {
        if (list_node_id == kInvalidSemanticId ||
            item_token == kInvalidVirtualSemanticItemToken) {
            return {};
        }
        return root_path() + "/" + std::to_string(list_node_id) +
               "/item/" + std::to_string(item_token);
    }

    bool operator==(const AccessibilityRootIdentity&) const noexcept = default;

private:
    AccessibilityRootIdentity(InstanceId application_instance_id,
                              ViewId view_id) noexcept
        : application_instance_id_(application_instance_id),
          view_id_(view_id) {}

    [[nodiscard]] std::string root_path() const {
        return "/org/nativeui/a11y/" + std::to_string(application_instance_id_) +
               "/" + std::to_string(view_id_);
    }

    InstanceId application_instance_id_{};
    ViewId view_id_{};
};

} // namespace ui::detail
