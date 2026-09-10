
        case PUGL_UNREALIZE:
            renderer_.reset();
            return PUGL_SUCCESS;

        case PUGL_CONFIGURE: {
            const Size physical{static_cast<float>(event->configure.width),
                                static_cast<float>(event->configure.height)};
            const auto logical = geometry_.configure(
                physical, static_cast<float>(puglGetScaleFactor(view_)));
            if (!logical) {
                // Zero/non-finite native extents are transient non-renderable
                // states. Preserve the last valid logical viewport until a
                // positive configure arrives.
                return PUGL_SUCCESS;
            }

            ui_.resize(*logical);
            // refresh_focus() replays focus_changed(true) on the focused text
            // editor, which republishes its candidate rectangle using the new
            // layout and scale factor exactly once at the platform boundary.
            ui_.refresh_focus(services_);
            preferred_measure_dirty_ = true;
            flush_preferred_size_notification();
            return PUGL_SUCCESS;
        }

        case PUGL_UPDATE:
            // XIM signals preedit-done before Xutf8LookupString returns a
            // possible commit. By the next Pugl update, any same-cycle text
            // event has already been consumed, so a remaining pending end is
            // a genuine composition cancellation.
            nativeuiImeFlushPendingCancel(ime_bridge_);
            flush_preferred_size_notification();
            return PUGL_SUCCESS;

        case PUGL_EXPOSE:
            if (!geometry_.renderable()) return PUGL_SUCCESS;
            if (!renderer_.render(ui_,
                                  services_,
                                  geometry_.physical_size(),
                                  geometry_.last_valid_scale())) {
                last_error_ = "Failed to initialize Skia Ganesh/OpenGL renderer";
                should_close_ = true;
                return PUGL_CREATE_CONTEXT_FAILED;
            }
            return PUGL_SUCCESS;

        case PUGL_CLOSE:
            // Notify an in-flight interaction immediately; destructor teardown
            // will then be idempotent. The Application callback only updates
            // per-Application bookkeeping; it never mutates the Pugl world.
            (void)ui_.cancel_pointer(services_);
            should_close_ = true;
            if (close_callback_) close_callback_();
            return PUGL_SUCCESS;

        case PUGL_FOCUS_IN:
            ui_.activate(services_);
            ui_.refresh_focus(services_);
            return PUGL_SUCCESS;

        case PUGL_FOCUS_OUT:
            ui_.deactivate(services_);
            set_text_input(false, {}, 0.0f);
            return PUGL_SUCCESS;

        case PUGL_KEY_PRESS:
        case PUGL_KEY_RELEASE: {
            InputEvent input{};
            input.type = event->type == PUGL_KEY_PRESS ? InputType::KeyDown : InputType::KeyUp;
            apply_modifiers(input, event->key.state);
            const auto tab = detail::normalize_tab_key(event->key.key, input.shift);
            input.key = tab.is_tab ? Key::Tab : translate_key(event->key.key, event->key.state);
            input.shift = tab.shift;
            if (input.type == InputType::KeyDown && input.key == Key::Quit) {
                should_close_ = true;
                if (close_callback_) close_callback_();
            } else if (input.key != Key::None) {
                ui_.dispatch(input, services_);
            }
            return PUGL_SUCCESS;
        }

        case PUGL_TEXT: {
            const std::string_view text{event->text.string};
            if (nativeuiImeConsumePuglText(ime_bridge_, text.data(), text.size())) {
                return PUGL_SUCCESS;
            }

            InputEvent input{};
            input.type = InputType::TextInput;
            input.text.assign(text);
            apply_modifiers(input, event->text.state);
            ui_.dispatch(input, services_);
            return PUGL_SUCCESS;
        }

        case PUGL_BUTTON_PRESS:
        case PUGL_BUTTON_RELEASE: {
            if (event->button.button != 0) return PUGL_SUCCESS;
            InputEvent input{};
            input.type = event->type == PUGL_BUTTON_PRESS ? InputType::PointerDown : InputType::PointerUp;
            input.position = detail::physical_to_logical_point(
                {static_cast<float>(event->button.x), static_cast<float>(event->button.y)},
                geometry_.last_valid_scale());
            apply_modifiers(input, event->button.state);

            if (event->type == PUGL_BUTTON_PRESS) {
                const double now = puglGetTime(world_);
                const float dx = input.position.x - last_click_position_.x;
                const float dy = input.position.y - last_click_position_.y;
                const bool close_in_time = now - last_click_time_ <= kMultiClickSeconds;
                const bool close_in_space = std::sqrt(dx * dx + dy * dy) <= kMultiClickDistance;
                click_count_ = close_in_time && close_in_space ? std::min(3, click_count_ + 1) : 1;
                last_click_time_ = now;
                last_click_position_ = input.position;
            }
            input.clicks = click_count_;
            ui_.dispatch(input, services_);
            return PUGL_SUCCESS;
        }

        case PUGL_MOTION: {
            InputEvent input{};
            input.type = InputType::PointerMove;
            input.position = detail::physical_to_logical_point(
                {static_cast<float>(event->motion.x), static_cast<float>(event->motion.y)},
                geometry_.last_valid_scale());
            if (have_pointer_position_) {
                input.delta = Point{input.position.x - last_pointer_position_.x,
                                    input.position.y - last_pointer_position_.y};
            }
            last_pointer_position_ = input.position;
            have_pointer_position_ = true;
            apply_modifiers(input, event->motion.state);
            ui_.dispatch(input, services_);
            return PUGL_SUCCESS;
        }

        case PUGL_SCROLL: {
            InputEvent input{};
            input.type = InputType::PointerWheel;
            input.position = detail::physical_to_logical_point(
                {static_cast<float>(event->scroll.x), static_cast<float>(event->scroll.y)},
                geometry_.last_valid_scale());
            input.delta = Point{static_cast<float>(event->scroll.dx),
                                static_cast<float>(event->scroll.dy)};
            apply_modifiers(input, event->scroll.state);
            ui_.dispatch(input, services_);
            return PUGL_SUCCESS;
        }

        case PUGL_TIMER:
            if (event->timer.id == kCaretTimerId) {
                InputEvent input{};
                input.type = InputType::Tick;
                ui_.dispatch(input, services_);
            }
            return PUGL_SUCCESS;

        case PUGL_DATA_OFFER: {
            const auto clipboard = event->offer.clipboard;
            const auto count = puglGetNumClipboardTypes(view_, clipboard);

            if (clipboard == PUGL_CLIPBOARD_DRAG) {
                InputEvent input{};
                input.type = InputType::DropOffer;
                input.position = detail::physical_to_logical_point(
                    {static_cast<float>(event->offer.x), static_cast<float>(event->offer.y)},
                    geometry_.last_valid_scale());
                input.drop_types.reserve(count);
                for (uint32_t i = 0; i < count; ++i) {
                    if (const char* type = puglGetClipboardType(view_, clipboard, i)) {
                        input.drop_types.emplace_back(type);
                    }
                }

                active_drop_offer_ = &event->offer;
                drop_offer_decided_ = false;
                (void)ui_.dispatch(input, services_);
                if (!drop_offer_decided_) {
                    (void)puglRejectOffer(view_,
                                          &event->offer,
                                          0,
                                          0,
                                          std::numeric_limits<uint32_t>::max(),
                                          std::numeric_limits<uint32_t>::max());
                }
                active_drop_offer_ = nullptr;
                drop_offer_decided_ = false;
                return PUGL_SUCCESS;
            }

            // Clipboard paste remains a separate text-input path.
            for (uint32_t i = 0; i < count; ++i) {
                const char* type = puglGetClipboardType(view_, clipboard, i);
                if (type && std::strncmp(type, "text/", 5) == 0) {
                    return puglAcceptOffer(view_,
                                           &event->offer,
                                           i,
                                           PUGL_DATA_ACTION_COPY,
                                           0,
                                           0,
                                           std::numeric_limits<uint32_t>::max(),
                                           std::numeric_limits<uint32_t>::max());
                }
            }
            return PUGL_SUCCESS;
        }

        case PUGL_DATA: {
            const auto clipboard = event->data.clipboard;
            const auto type_index = event->data.typeIndex;
            const char* type = puglGetClipboardType(view_, clipboard, type_index);
            if (!type) return PUGL_SUCCESS;

            size_t size = 0;
            const void* data = puglGetClipboard(view_, clipboard, type_index, &size);
            if (!data || !size) return PUGL_SUCCESS;

            if (clipboard == PUGL_CLIPBOARD_DRAG) {
                InputEvent input{};
                input.type = InputType::DropData;
                input.position = detail::physical_to_logical_point(
                    {static_cast<float>(event->data.x), static_cast<float>(event->data.y)},
                    geometry_.last_valid_scale());
                input.drop_type = type;
                const auto* bytes = static_cast<const std::uint8_t*>(data);
                input.drop_data.assign(bytes, bytes + size);
                (void)ui_.dispatch(input, services_);
                return PUGL_SUCCESS;
            }

            if (std::strncmp(type, "text/", 5) == 0) {
                InputEvent input{};
                input.type = InputType::TextInput;
                input.text.assign(static_cast<const char*>(data), size);
                ui_.dispatch(input, services_);
            }
            return PUGL_SUCCESS;
        }

        default:
            return PUGL_SUCCESS;
        }
    }

    UI& ui_;
    PlatformServices& services_;
    PuglWorld* world_{};
    bool owns_world_{};
    PuglView* view_{};
    NativeUIImeBridge* ime_bridge_{};
    SkiaGlRenderer renderer_;
    detail::ViewGeometryState geometry_;
    detail::PreferredSizeState preferred_size_state_;
    PreferredSizeCallback preferred_size_callback_;
    bool preferred_measure_dirty_{};
    bool should_close_{};
    bool text_input_active_{};
    Rect text_input_logical_area_{};
    float text_input_logical_cursor_offset_{};
    Rect text_input_physical_area_{};
    float text_input_physical_cursor_offset_{};
    bool have_pointer_position_{};
    Point last_pointer_position_{};
    double last_click_time_{-1000.0};
    Point last_click_position_{};
    int click_count_{};
    const PuglDataOfferEvent* active_drop_offer_{};
    bool drop_offer_decided_{};
    std::string last_error_;
    std::function<void()> close_callback_;
};

} // namespace
