#pragma once

#include <cstddef>
#include <span>
#include <string_view>

namespace ui {

/// One entry in an application/generated immutable embedded-resource table.
///
/// This value is a pair of borrowed views; it owns neither the identifier nor
/// the payload. Copying an entry copies only the two views. The backing
/// character and byte storage must therefore outlive every table, ResourceManager
/// and ResourceView that can still reference the entry.
///
/// ResourceManager accepts an empty table, but each entry in a non-empty table
/// must have a non-empty ID and the complete table must be strictly ascending
/// and unique according to NativeUI's exact unsigned-byte lexicographic ID
/// ordering. Payloads may be empty: an empty payload is a present resource, not
/// a missing resource.
struct EmbeddedResourceEntry {
  /// Borrowed exact resource identifier.
  ///
  /// IDs are not normalized, case-folded or path-decoded by ResourceManager.
  /// The referenced characters must remain alive and immutable for the complete
  /// lifetime of every manager/view that borrows this entry.
  std::string_view id;

  /// Borrowed encoded payload bytes.
  ///
  /// A zero-length span is valid and represents a present empty resource. The
  /// referenced bytes must remain alive and immutable for the complete lifetime
  /// of every manager/view that borrows this entry.
  std::span<const std::byte> bytes;
};

} // namespace ui
