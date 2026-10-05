#pragma once

#include <nativeui/dispatcher.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace ui {
class UI;
namespace detail {
struct ToastOwnerToken;
struct ToastLifetimeToken;
} // namespace detail

class ToastHandle final {
  public:
    ToastHandle() = default;
    [[nodiscard]] bool valid() const noexcept;
    explicit operator bool() const noexcept { return valid(); }
    [[nodiscard]] bool operator==(const ToastHandle &other) const noexcept;

  private:
    friend class Toast;
    ToastHandle(std::weak_ptr<const detail::ToastOwnerToken> owner,
                std::weak_ptr<const detail::ToastLifetimeToken> lifetime,
                std::uint64_t id) noexcept;
    std::weak_ptr<const detail::ToastOwnerToken> owner_;
    std::weak_ptr<const detail::ToastLifetimeToken> lifetime_;
    std::uint64_t id_{};
};
struct ToastSpec {
    std::string message;
    std::string action_label;
    std::function<void()> action;
    std::optional<std::chrono::milliseconds> duration;
};
enum class ToastShowStatus { Shown, InvalidSpec, Unavailable };
struct ToastShowResult {
    ToastShowStatus status{ToastShowStatus::Unavailable};
    ToastHandle handle;
};

/// Instance-owned notifications. UI and Dispatcher are borrowed through weak
/// lifetime capabilities; no message/action survives owner deactivation.
class Toast final {
  public:
    Toast(UI &ui, Dispatcher dispatcher);
    Toast(const Toast &) = delete;
    Toast &operator=(const Toast &) = delete;
    Toast(Toast &&) = delete;
    Toast &operator=(Toast &&) = delete;
    ~Toast() noexcept;
    [[nodiscard]] ToastShowResult show(ToastSpec value);
    bool dismiss(ToastHandle handle);

  private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};
} // namespace ui
