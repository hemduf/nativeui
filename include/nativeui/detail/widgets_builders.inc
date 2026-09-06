class Label {
public:
    explicit Label(std::string text) : text_(std::move(text)) {}

    Label&& size(float value) && {
        style_.size = std::max(0.0f, value);
        return std::move(*this);
    }

    Label&& color(Color value) && {
        style_.color = value;
        return std::move(*this);
    }

    Label&& align(TextAlign value) && {
        style_.align = value;
        return std::move(*this);
    }

    Label&& weight(FontWeight value) && {
        style_.weight = value;
        return std::move(*this);
    }

    Label&& bold(bool value = true) && {
        style_.weight = value ? FontWeight::Bold : FontWeight::Regular;
        return std::move(*this);
    }

    Label&& style(TextStyle value) && {
        style_ = value;
        return std::move(*this);
    }

    Spec spec() && {
        auto text = std::move(text_);
        const auto style = style_;
        return Spec{
            [text = std::move(text), style]() mutable {
                return std::make_unique<LabelComponent>(std::move(text), style);
            },
            {}};
    }

private:
    std::string text_;
    TextStyle style_{};
};

using TextLabel = Label;

class Header {
public:
    explicit Header(std::string title) : title_(std::move(title)) {}

    Spec spec() && {
        const auto title = std::move(title_);
        return Spec{
            [title] { return std::make_unique<HeaderComponent>(title); },
            {}};
    }

private:
    std::string title_;
};


class Canvas {
public:
    using DrawCallback = CanvasComponent::DrawCallback;
    using InputCallback = CanvasComponent::InputCallback;

    Canvas(Size size, DrawCallback draw)
        : size_(size), draw_(std::move(draw)) {}

    Canvas(float width, float height, DrawCallback draw)
        : Canvas(Size{width, height}, std::move(draw)) {}

    template <class Callback>
    Canvas&& on_input(Callback&& callback) && {
        using CallbackType = std::decay_t<Callback>;
        using Result = std::invoke_result_t<
            CallbackType&, const InputEvent&, CanvasInputContext&>;

        if constexpr (std::is_same_v<Result, EventResult>) {
            input_ = std::forward<Callback>(callback);
        } else {
            static_assert(std::is_void_v<Result>,
                          "Canvas input callbacks must return void or ui::EventResult");
            input_ = [callback = CallbackType(std::forward<Callback>(callback))](
                         const InputEvent& event, CanvasInputContext& context) mutable {
                std::invoke(callback, event, context);
                return EventResult::Handled;
            };
        }

        focusable_ = true;
        return std::move(*this);
    }

    Canvas&& focusable(bool value = true) && {
        focusable_ = value;
        return std::move(*this);
    }

    Spec spec() && {
        const auto size = size_;
        auto draw = std::move(draw_);
        auto input = std::move(input_);
        const bool focusable = focusable_;

        return Spec{
            [size, draw = std::move(draw), input = std::move(input), focusable]() mutable {
                return std::make_unique<CanvasComponent>(
                    size, std::move(draw), std::move(input), focusable);
            },
            {}};
    }

private:
    Size size_{};
    DrawCallback draw_;
    InputCallback input_;
    bool focusable_{};
};


class Knob {
public:
    Knob(std::string label, State<float>& state)
        : label_(std::move(label)), state_(&state) {}

    Knob&& range(float minimum, float maximum) && {
        minimum_ = minimum;
        maximum_ = maximum;
        return std::move(*this);
    }

    Spec spec() && {
        auto label = std::move(label_);
        auto* state = state_;
        const float minimum = minimum_;
        const float maximum = maximum_;
        return Spec{[label = std::move(label), state, minimum, maximum]() mutable {
                        return std::make_unique<KnobComponent>(std::move(label), *state, minimum, maximum);
                    },
                    {}};
    }

private:
    std::string label_;
    State<float>* state_{};
    float minimum_{0.0f};
    float maximum_{1.0f};
};


class Toggle {
public:
    Toggle(std::string label, State<bool>& state)
        : label_(std::move(label)), state_(&state) {}

    Spec spec() && {
        auto label = std::move(label_);
        auto* state = state_;
        return Spec{[label = std::move(label), state]() mutable {
                        return std::make_unique<ToggleComponent>(std::move(label), *state);
                    },
                    {}};
    }

private:
    std::string label_;
    State<bool>* state_{};
};


class TextInput {
public:
    using SubmitCallback = TextInputComponent::SubmitCallback;

    TextInput(std::string label, State<std::string>& state)
        : label_(std::move(label)), state_(&state) {}

    TextInput&& placeholder(std::string value) && {
        placeholder_ = std::move(value);
        return std::move(*this);
    }

    TextInput&& max_length(std::size_t value) && {
        max_length_ = value;
        return std::move(*this);
    }

    TextInput&& on_submit(SubmitCallback callback) && {
        on_submit_ = std::move(callback);
        return std::move(*this);
    }

    Spec spec() && {
        auto label = std::move(label_);
        auto placeholder = std::move(placeholder_);
        auto callback = std::move(on_submit_);
        auto* state = state_;
        const auto max_length = max_length_;
        return Spec{[label = std::move(label), state, placeholder = std::move(placeholder),
                     max_length, callback = std::move(callback)]() mutable {
                        return std::make_unique<TextInputComponent>(
                            std::move(label), *state, std::move(placeholder), max_length, std::move(callback));
                    },
                    {}};
    }

private:
    std::string label_;
    State<std::string>* state_{};
    std::string placeholder_;
    std::size_t max_length_{256};
    SubmitCallback on_submit_;
};


} // namespace ui
