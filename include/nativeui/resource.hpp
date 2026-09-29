#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace ui {

/// Supplies encoded resource bytes by application-defined identifier.
///
/// NativeUI deliberately leaves storage policy to the application. Providers
/// can source bytes from files, bundles, archives, generated memory or any
/// other backend without coupling widgets or resource caches to filesystem I/O.
///
/// The interface owns no registry, cache or scheduler. Implementations decide
/// how identifiers map to storage and whether loading performs allocation, I/O
/// or synchronization. Calls are therefore ordinary application/resource-
/// preparation work unless a concrete provider documents a stronger contract.
///
/// ResourceProvider instances are normally used from the UI/resource-preparation
/// domain. The base class provides no cross-thread synchronization and makes no
/// audio-real-time guarantee. Implementations that are shared across threads
/// must document and enforce their own synchronization policy.
class ResourceProvider {
public:
    /// Destroy the provider through the polymorphic base.
    ///
    /// Destruction has normal C++ ownership semantics. NativeUI does not retain
    /// or cancel outstanding provider work because the base interface defines no
    /// asynchronous operation.
    virtual ~ResourceProvider() = default;

    /// Load one encoded resource payload by exact application-defined ID.
    ///
    /// `resource_id` is a borrowed string view valid only for this call. The
    /// base contract performs no normalization, case folding, path decoding or
    /// fallback lookup; an implementation may define such behavior explicitly,
    /// but callers must not assume it.
    ///
    /// Return an owned byte vector on success. A present zero-length resource is
    /// represented by an engaged empty vector. Return `std::nullopt` when this
    /// provider cannot supply the requested identifier.
    ///
    /// Ownership of returned bytes transfers to the caller and is independent
    /// of the provider after return. Implementations may allocate, perform I/O,
    /// synchronize, or throw ordinary C++ exceptions; the base interface does
    /// not translate failures into `std::nullopt`. Calls are synchronous and
    /// there is no retry, caching or callback reentrancy contract at this layer.
    [[nodiscard]] virtual std::optional<std::vector<std::byte>> load(
        std::string_view resource_id) = 0;
};

} // namespace ui
