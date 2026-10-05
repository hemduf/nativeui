#pragma once

#include <nativeui/text_input.hpp>

#include <memory>
#include <optional>
#include <utility>

namespace ui::detail {
struct TextInputSnapshot {
  std::string text;
  std::size_t anchor{};
  std::size_t cursor{};
  bool composition_active{};
  bool focused{};
  bool read_only{};
  bool allow_edit_commit{true};
  std::uint64_t edit_generation{};
  std::function<bool()> mutation_guard;
};
class TextInputSession;
struct TextInputPolicy {
  // Pure retained options; measurement and semantic reads never call a policy.
  bool read_only{};
  bool focusable{true};
  bool paint_chrome{true};
  bool paint_label{true};
  std::optional<Color> border_override;
  // A null result means pass-through and must not call application code or
  // change retained structure. A present result ends dispatch immediately.
  // Context is borrowed for this call only and must never be retained.
  std::function<std::optional<EventResult>(const InputEvent &, InputContext &,
                                           const TextInputSnapshot &)>
      before_input;
  // An accepted user edit, including equal-text replacement. Never invoked
  // for source/session replacement; edit_generation is durable before publish.
  std::function<void(TextInputSnapshot)> committed_edit;
  // Invoked last, after editor focus/IME/capture/invalidation invariants are
  // restored. This callback receives owned text, never a borrowed context.
  std::function<void(bool, TextInputSnapshot)> focus_changed;
};
class TextInputSession {
public:
  ~TextInputSession();
  TextInputSession(const TextInputSession &) = delete;
  TextInputSession &operator=(const TextInputSession &) = delete;
  [[nodiscard]] bool mounted() const noexcept;
  [[nodiscard]] TextInputSnapshot snapshot() const;
  // Edit the local buffer only. Controllers decide separately whether to
  // publish their backing Binding; source notifications cannot trigger submit.
  void
  replace(std::string text,
          std::optional<std::pair<std::size_t, std::size_t>> selection = {},
          bool reset_history = true);
  // Reconcile only from the authoritative source, without writes or callbacks.
  void refresh_source();
  void select(std::size_t begin, std::size_t end);
  void cancel_composition();
  void cancel_capture() noexcept;
  void reset_baseline();
  void request_focus();

private:
  friend struct TextInputAccess;
  friend class ::ui::TextInputComponent;
  struct Storage;
  TextInputSession();
  std::unique_ptr<Storage> storage_;
};
struct TextInputAccess {
  static void initialize(TextInputComponent &editor);
  static void mounted(TextInputComponent &editor, MountContext &context);
  static void detached(TextInputComponent &editor) noexcept;
  [[nodiscard]] static TextInputSnapshot
  snapshot(const TextInputComponent &editor);
  // Configure only before mount, once per fresh editor constructed by a
  // parent's children_factory. The editor owns session and policy; a controller
  // holds only weak session handles, so no ownership cycle is possible.
  [[nodiscard]] static std::weak_ptr<TextInputSession>
  configure(TextInputComponent &editor,
            std::shared_ptr<TextInputPolicy> policy);
};
} // namespace ui::detail
