/// Detached builder for one retained text label.
///
/// The builder owns its UTF-8 text and TextStyle value. Configuration mutates
/// only this detached value; retained/UI work starts when the resulting Spec is
/// materialized. Builder operations may allocate when copying/moving strings or
/// fallback-family vectors and are not audio/DSP real-time APIs.
class Label {
public:
    /// Take ownership of the label text. Bytes are retained verbatim.
    explicit Label(std::string text) : text_(std::move(text)) {}

    /// Set text size in logical UI units; negative values clamp to zero.
    Label&& size(float value) && {
        style_.size = std::max(0.0f, value);
        return std::move(*this);
    }

    /// Replace the owned text color value without color-space conversion.
    Label&& color(Color value) && {
        style_.color = value;
        return std::move(*this);
    }

    /// Select text alignment inside the eventual retained label bounds.
    Label&& align(TextAlign value) && {
        style_.align = value;
        return std::move(*this);
    }

    /// Set the explicit font weight token.
    Label&& weight(FontWeight value) && {
        style_.weight = value;
        return std::move(*this);
    }

    /// Convenience toggle between Bold and Regular weight.
    Label&& bold(bool value = true) && {
        style_.weight = value ? FontWeight::Bold : FontWeight::Regular;
        return std::move(*this);
    }

    /// Set the explicit font slant token.
    Label&& slant(FontSlant value) && {
        style_.slant = value;
        return std::move(*this);
    }

    /// Convenience toggle between Italic and Upright slant.
    Label&& italic(bool value = true) && {
        style_.slant = value ? FontSlant::Italic : FontSlant::Upright;
        return std::move(*this);
    }

    /// Take ownership of the preferred font-family name.
    Label&& family(std::string value) && {
        style_.family = std::move(value);
        return std::move(*this);
    }

    /// Take ownership of ordered fallback family names.
    Label&& fallback_families(std::vector<std::string> value) && {
        style_.fallback_families = std::move(value);
        return std::move(*this);
    }

    /// Replace the complete owned TextStyle, overriding earlier fluent fields.
    Label&& style(TextStyle value) && {
        style_ = std::move(value);
        return std::move(*this);
    }

    /// Consume this builder and transfer owned text/style into a retained Spec.
    ///
    /// The returned specification borrows nothing from this Label object.
    Spec spec() && {
        auto text = std::move(text_);
        auto style = std::move(style_);
        return Spec{
            [text = std::move(text), style = std::move(style)]() mutable {
                return std::make_unique<LabelComponent>(std::move(text), std::move(style));
            },
            {}};
    }

private:
    std::string text_;
    TextStyle style_{};
};

/// Compatibility alias for Label; it has exactly the same ownership contract.
using TextLabel = Label;

/// Detached builder for the standard retained section/header text treatment.
///
/// The title is owned by the builder and transferred into the produced Spec.
class Header {
public:
    /// Take ownership of UTF-8 title bytes.
    explicit Header(std::string title) : title_(std::move(title)) {}

    /// Consume this builder and return an independently owned retained recipe.
    Spec spec() && {
        const auto title = std::move(title_);
        return Spec{
            [title] { return std::make_unique<HeaderComponent>(title); },
            {}};
    }

private:
    std::string title_;
};


/// Detached custom-drawing builder with optional retained input handling.
///
/// Size is expressed in logical UI pixels. Draw/input callbacks are owned by the
/// produced component and execute synchronously in the owning UI/render/input
/// domain. Callback contexts are borrowed for the invocation only and must not
/// escape. Callbacks may allocate, throw or re-enter application code, so Canvas
/// is not an audio/DSP real-time boundary.
class Canvas {
public:
    /// Draw callback invoked with a callback-scoped CanvasContext2D borrow.
    using DrawCallback = CanvasComponent::DrawCallback;
    /// Input callback invoked with logical/local event geometry and a borrowed
    /// CanvasInputContext. EventResult controls normal retained propagation.
    using InputCallback = CanvasComponent::InputCallback;

    /// Take ownership of the draw callback for the requested logical size.
    Canvas(Size size, DrawCallback draw)
        : size_(size), draw_(std::move(draw)) {}

    /// Convenience constructor using logical width/height.
    Canvas(float width, float height, DrawCallback draw)
        : Canvas(Size{width, height}, std::move(draw)) {}

    /// Install an owned input callback and make the canvas focusable.
    ///
    /// A callback returning EventResult controls propagation directly. A void
    /// callback is adapted to EventResult::Handled. Callback/context references
    /// are valid only for the synchronous event invocation.
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

    /// Explicitly enable/disable retained keyboard-focus participation.
    Canvas&& focusable(bool value = true) && {
        focusable_ = value;
        return std::move(*this);
    }

    /// Consume this builder and transfer callbacks/configuration into a Spec.
    ///
    /// The returned Spec owns the callbacks; it does not borrow this Canvas.
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


/// Retained rotary-value builder backed by a Binding<float>.
///
/// The builder/component owns the Binding control-block handle, not the source
/// State object. If the source State is destroyed, the binding becomes logically
/// invalid: reads retain the last committed value while writes/observation no-op.
/// Editing callbacks are synchronous UI/input callbacks and may re-enter.
class Knob {
public:
    /// Take ownership of label text and a copyable binding handle.
    Knob(std::string label, Binding<float> state)
        : label_(std::move(label)), state_(std::move(state)) {}

    /// Convenience overload obtaining a Binding from the supplied State.
    Knob(std::string label, State<float>& state)
        : Knob(std::move(label), state.binding()) {}

    /// Configure the scalar editing domain. If maximum <= minimum, the retained
    /// component deterministically uses minimum + 1 as its effective maximum.
    Knob&& range(float minimum, float maximum) && {
        minimum_ = minimum;
        maximum_ = maximum;
        return std::move(*this);
    }

    /// Take ownership of begin/update/end edit callbacks for UI-thread delivery.
    Knob&& on_edit(EditCallbacks<float> callbacks) && {
        callbacks_ = std::move(callbacks);
        return std::move(*this);
    }

    /// Enable or disable pointer-wheel editing; disabled is the default.
    Knob&& wheel_enabled(bool value = true) && {
        wheel_enabled_ = value;
        return std::move(*this);
    }

    /// Consume this builder and transfer label/binding/callback ownership.
    Spec spec() && {
        auto label = std::move(label_);
        auto state = std::move(state_);
        const float minimum = minimum_;
        const float maximum = maximum_;
        auto callbacks = std::move(callbacks_);
        const bool wheel_enabled = wheel_enabled_;
        return Spec{[label = std::move(label), state = std::move(state), minimum, maximum,
                     callbacks = std::move(callbacks), wheel_enabled]() mutable {
                        return std::make_unique<KnobComponent>(
                            std::move(label), std::move(state), minimum, maximum,
                            std::move(callbacks), wheel_enabled);
                    },
                    {}};
    }

private:
    std::string label_;
    Binding<float> state_;
    float minimum_{0.0f};
    float maximum_{1.0f};
    EditCallbacks<float> callbacks_;
    bool wheel_enabled_{};
};


/// Retained boolean toggle builder backed by an owned Binding<bool> handle.
///
/// Styling and EditCallbacks are transferred into the retained component. State
/// mutation and callbacks are synchronous UI/input work and may re-enter.
class Toggle {
public:
    /// Take ownership of label text and a copyable binding handle.
    Toggle(std::string label, Binding<bool> state)
        : label_(std::move(label)), state_(std::move(state)) {}

    /// Convenience overload obtaining a Binding from the supplied State.
    Toggle(std::string label, State<bool>& state)
        : Toggle(std::move(label), state.binding()) {}

    /// Replace the complete owned ToggleStyle value.
    Toggle&& style(ToggleStyle value) && {
        style_ = std::move(value);
        return std::move(*this);
    }

    /// Take ownership of synchronous edit callbacks.
    Toggle&& on_edit(EditCallbacks<bool> callbacks) && {
        callbacks_ = std::move(callbacks);
        return std::move(*this);
    }

    /// Consume this builder and transfer label/binding/style/callback ownership.
    Spec spec() && {
        auto label = std::move(label_);
        auto state = std::move(state_);
        auto style = std::move(style_);
        auto callbacks = std::move(callbacks_);
        return Spec{[label = std::move(label), state = std::move(state), style = std::move(style),
                     callbacks = std::move(callbacks)]() mutable {
                        return std::make_unique<ToggleComponent>(
                            std::move(label), std::move(state), std::move(style), std::move(callbacks));
                    },
                    {}};
    }

private:
    std::string label_;
    Binding<bool> state_;
    ToggleStyle style_;
    EditCallbacks<bool> callbacks_;
};


/// Single-line retained UTF-8 editor builder.
///
/// The retained component owns label/placeholder/style/callback values and one
/// Binding<std::string> handle. Text positions and max-length behavior follow
/// TextEditModel. Editing, IME, clipboard and callbacks are UI/main-thread work
/// and may allocate or re-enter; this is not an audio/DSP real-time API.
class TextInput {
public:
    /// Synchronous Enter callback. The string reference borrows the component's
    /// current model text for the callback only and must not be retained.
    using SubmitCallback = TextInputComponent::SubmitCallback;
    /// Pre-built-in KeyDown hook. Handled suppresses the standard key command;
    /// Ignored preserves normal TextInput processing.
    using KeyDownCallback = TextInputComponent::KeyDownCallback;

    /// Take ownership of label text and the binding control-block handle.
    TextInput(std::string label, Binding<std::string> state)
        : label_(std::move(label)), state_(std::move(state)) {}

    /// Convenience overload obtaining a Binding from the supplied State.
    TextInput(std::string label, State<std::string>& state)
        : TextInput(std::move(label), state.binding()) {}

    /// Take ownership of placeholder UTF-8 text.
    TextInput&& placeholder(std::string value) && {
        placeholder_ = std::move(value);
        return std::move(*this);
    }

    /// Set the future insertion/composition budget in TextEditModel code-point-
    /// like units. Zero means unlimited; existing source text is not truncated.
    TextInput&& max_length(std::size_t value) && {
        max_length_ = value;
        return std::move(*this);
    }

    /// Take ownership of the Enter-submit callback.
    ///
    /// It executes synchronously during input dispatch and may re-enter or
    /// destroy surrounding application objects; callers must not retain its
    /// borrowed string reference.
    TextInput&& on_submit(SubmitCallback callback) && {
        on_submit_ = std::move(callback);
        return std::move(*this);
    }

    /// Handle KeyDown before built-in cursor/edit commands.
    ///
    /// Return Ignored to preserve standard processing or Handled to suppress it.
    /// The event is borrowed for the synchronous callback; exceptions propagate
    /// from input dispatch after normal C++ unwinding.
    TextInput&& on_key_down(KeyDownCallback callback) && {
        on_key_down_ = std::move(callback);
        return std::move(*this);
    }

    /// Replace the complete owned TextInputStyle value.
    TextInput&& style(TextInputStyle value) && {
        style_ = std::move(value);
        return std::move(*this);
    }

    /// Consume this builder and transfer all owned configuration into a Spec.
    ///
    /// The returned specification borrows nothing from this TextInput object.
    Spec spec() && {
        auto label = std::move(label_);
        auto state = std::move(state_);
        auto placeholder = std::move(placeholder_);
        auto callback = std::move(on_submit_);
        auto key_down = std::move(on_key_down_);
        auto style = std::move(style_);
        const auto max_length = max_length_;
        return Spec{[label = std::move(label), state = std::move(state), placeholder = std::move(placeholder),
                     max_length, callback = std::move(callback), key_down = std::move(key_down),
                     style = std::move(style)]() mutable {
                        return std::make_unique<TextInputComponent>(
                            std::move(label), std::move(state), std::move(placeholder), max_length,
                            std::move(callback), std::move(key_down), std::move(style));
                    },
                    {}};
    }

private:
    std::string label_;
    Binding<std::string> state_;
    std::string placeholder_;
    std::size_t max_length_{256};
    SubmitCallback on_submit_;
    KeyDownCallback on_key_down_;
    TextInputStyle style_;
};


/// Multi-line retained UTF-8 editor builder.
///
/// Ownership/threading matches TextInput: configuration and Binding handles are
/// owned by the retained component, while editing/IME/clipboard work stays in
/// the UI/main-thread domain and may allocate.
class TextArea {
public:
    /// Take ownership of label text and the binding control-block handle.
    TextArea(std::string label, Binding<std::string> state)
        : label_(std::move(label)), state_(std::move(state)) {}

    /// Convenience overload obtaining a Binding from the supplied State.
    TextArea(std::string label, State<std::string>& state)
        : TextArea(std::move(label), state.binding()) {}

    /// Take ownership of placeholder UTF-8 text.
    TextArea&& placeholder(std::string value) && {
        placeholder_ = std::move(value);
        return std::move(*this);
    }

    /// Set the future insertion/composition budget; zero means unlimited.
    /// Existing source text is not truncated by builder configuration.
    TextArea&& max_length(std::size_t value) && {
        max_length_ = value;
        return std::move(*this);
    }

    /// Replace the complete owned TextAreaStyle value.
    TextArea&& style(TextAreaStyle value) && {
        style_ = std::move(value);
        return std::move(*this);
    }

    /// Consume this builder and transfer all owned configuration into a Spec.
    ///
    /// The returned specification borrows nothing from this TextArea object.
    Spec spec() && {
        auto label = std::move(label_);
        auto state = std::move(state_);
        auto placeholder = std::move(placeholder_);
        auto style = std::move(style_);
        const auto max_length = max_length_;
        return Spec{[label = std::move(label), state = std::move(state),
                     placeholder = std::move(placeholder), max_length,
                     style = std::move(style)]() mutable {
                        return std::make_unique<TextAreaComponent>(
                            std::move(label), std::move(state), std::move(placeholder), max_length,
                            std::move(style));
                    },
                    {}};
    }

private:
    std::string label_;
    Binding<std::string> state_;
    std::string placeholder_;
    std::size_t max_length_{};
    TextAreaStyle style_;
};


} // namespace ui
