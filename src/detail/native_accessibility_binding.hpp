#pragma once

#include <nativeui/detail/semantic_action_view_binding.hpp>
#include <nativeui/detail/semantic_native_publication.hpp>

#include <memory>

namespace ui::detail {

/// Borrowed per-view endpoints handed to one native accessibility bridge.
///
/// The owning ViewCore builds this value from the exact
/// SemanticNativeViewBridge it has already bound, so native reads and native
/// actions always belong to the same view. The bridge copies both weak handles
/// during attach and never retains this binding, the view bridge, the retained
/// Tree, or any native view: after attach the only ownership edges are the
/// per-view weak publication source and the per-view weak action endpoint.
///
/// A missing publication source fails the attach closed (no native accessibility
/// object is created). A missing action endpoint is valid for a read-only view;
/// all native actions then fail closed while queries continue to work.
struct NativeAccessibilityAttachBinding final {
    std::weak_ptr<const SemanticNativePublicationSource> publication_source;
    std::weak_ptr<const SemanticActionViewEndpoint> action_endpoint;
};

} // namespace ui::detail
