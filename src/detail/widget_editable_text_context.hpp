#pragma once
#include <cstdint>
#include <functional>
#include <memory>
namespace ui::detail {
// A collection row supplies weak/key-based functions. No Node/collection borrow
// is retained; selection, scroll, reorder and removal advance its generation.
struct EditableTextRowContext {
  std::function<bool()> selected;
  std::function<void()> request_select;
  std::function<std::uint64_t()> interaction_generation;
  std::function<bool()> attached;
};
class EditableTextContextConsumer {
public:
  virtual ~EditableTextContextConsumer() = default;
  virtual void bind_editable_row_context(
      std::shared_ptr<const EditableTextRowContext>) = 0;
};
} // namespace ui::detail
