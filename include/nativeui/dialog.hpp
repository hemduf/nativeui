#pragma once

#include <nativeui/ui.hpp>

#include <functional>
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
};

class Dialog final {
public:
    using Completion = std::function<void(DialogResult)>;

    explicit Dialog(UI& ui) noexcept : ui_(&ui) {}

    Dialog(const Dialog&) = delete;
    Dialog& operator=(const Dialog&) = delete;
    Dialog(Dialog&&) = delete;
    Dialog& operator=(Dialog&&) = delete;

    ~Dialog() {
        if (active_) (void)close();
    }

    [[nodiscard]] DialogShowResult show(DialogSpec spec, Completion completion) {
        if (active_) return DialogShowResult::Busy;
        if (!valid_spec(spec)) return DialogShowResult::InvalidSpec;

        spec_ = std::move(spec);
        completion_ = std::move(completion);
        active_ = true;
        return DialogShowResult::Shown;
    }

    [[nodiscard]] bool active() const noexcept { return active_; }

    [[nodiscard]] bool close() {
        if (!active_) return false;

        active_ = false;
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
    bool active_{};
    DialogSpec spec_;
    Completion completion_;
};

} // namespace ui
