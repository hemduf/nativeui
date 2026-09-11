#pragma once

#include <nativeui/ui.hpp>

#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ui {

using DialogActionId = std::string;

enum class DialogResultKind {
    Action,
    Dismissed,
};

struct DialogResult {
    DialogResultKind kind{DialogResultKind::Dismissed};
    DialogActionId action_id;
};

enum class DialogActionRole {
    Normal,
    Default,
    Cancel,
};

struct DialogAction {
    DialogActionId id;
    std::string label;
    bool enabled{true};
    DialogActionRole role{DialogActionRole::Normal};
};

struct DialogSpec {
    std::string title;
    Spec body;
    std::vector<DialogAction> actions;
};

enum class DialogShowResult {
    Shown,
    Busy,
    InvalidSpec,
    Unavailable,
};

class Dialog final {
public:
    using Completion = std::function<void(DialogResult)>;

    explicit Dialog(UI& ui) noexcept
        : ui_(&ui), state_(ui.dialog_state_) {}

    Dialog(const Dialog&) = delete;
    Dialog& operator=(const Dialog&) = delete;
    Dialog(Dialog&&) = delete;
    Dialog& operator=(Dialog&&) = delete;

    ~Dialog() {
        if (active()) {
            (void)close();
        } else {
            // The owning UI may already have begun teardown. In that case the
            // shared state made this controller inert and the application
            // callback is deliberately discarded rather than emitted from a
            // destructor after the UI lifetime ended.
            completion_ = {};
            spec_ = {};
            overlay_ = {};
            generation_ = 0;
        }
    }

    [[nodiscard]] DialogShowResult show(DialogSpec spec, Completion completion) {
        if (!state_ || state_->ui_tearing_down || !ui_) {
            return DialogShowResult::Unavailable;
        }
        if (state_->active_generation != 0) return DialogShowResult::Busy;
        if (!valid_spec(spec)) return DialogShowResult::InvalidSpec;

        const auto generation = state_->acquire();
        if (generation == 0) {
            return state_->ui_tearing_down
                ? DialogShowResult::Unavailable
                : DialogShowResult::Busy;
        }

        OverlaySpec overlay;
        overlay.mode = OverlayMode::Modal;
        overlay.pointer_policy = OverlayPointerPolicy::Normal;
        overlay.placement = OverlayPlacement::Center;
        overlay.dismiss_on_escape = false;
        overlay.dismiss_on_outside_pointer_down = false;
        overlay.content = std::move(spec.body);

        auto handle = ui_->show_overlay(std::move(overlay));
        if (!handle.valid()) {
            (void)state_->release(generation);
            return DialogShowResult::Unavailable;
        }

        spec_ = std::move(spec);
        completion_ = std::move(completion);
        generation_ = generation;
        overlay_ = std::move(handle);
        return DialogShowResult::Shown;
    }

    [[nodiscard]] bool active() const noexcept {
        return state_ && state_->owns(generation_);
    }

    [[nodiscard]] bool close() {
        if (!state_ || !state_->owns(generation_)) return false;

        // Logical overlay close happens before the per-UI active slot is
        // released and before application code runs. T058 owns the retained
        // structural checkpoint; the later T063 completion unit tightens the
        // callback to run only after that checkpoint when close originates from
        // inside an input callback.
        if (ui_ && overlay_.valid()) (void)ui_->close_overlay(overlay_);
        overlay_ = {};

        if (!state_->release(generation_)) return false;
        generation_ = 0;
        spec_ = {};
        auto completion = std::move(completion_);
        completion_ = {};
        if (completion) completion(DialogResult{DialogResultKind::Dismissed, {}});
        return true;
    }

private:
    [[nodiscard]] static bool valid_spec(const DialogSpec& spec) {
        if (!spec.body.factory) return false;

        std::unordered_set<std::string> ids;
        bool has_default = false;
        bool has_cancel = false;
        for (const auto& action : spec.actions) {
            if (action.id.empty() || !ids.insert(action.id).second) return false;
            if (action.role == DialogActionRole::Default) {
                if (has_default) return false;
                has_default = true;
            } else if (action.role == DialogActionRole::Cancel) {
                if (has_cancel) return false;
                has_cancel = true;
            }
        }
        return true;
    }

    UI* ui_{};
    std::shared_ptr<detail::DialogState> state_;
    std::uint64_t generation_{};
    OverlayHandle overlay_;
    DialogSpec spec_;
    Completion completion_;
};

} // namespace ui
