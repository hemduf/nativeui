
        case PUGL_UNREALIZE:
            renderer_.reset();
            return PUGL_SUCCESS;

        case PUGL_CONFIGURE: {
            const float previous_scale = geometry_.last_valid_scale();
            const Size physical{static_cast<float>(event->configure.width),
                                static_cast<float>(event->configure.height)};
            const auto logical = detail::apply_authoritative_configure(
                geometry_,
                physical,
                static_cast<float>(puglGetScaleFactor(view_)),
                [this](Size viewport) { ui_.resize(viewport); });
            record_scale_observation(geometry_.last_scale_observation_valid());
            if (!logical) {
                renderer_.invalidate_content();
                return PUGL_SUCCESS;
            }

            if (geometry_.last_valid_scale() != previous_scale &&
                !apply_size_constraints()) {
                last_error_ = "Failed to refresh logical window size constraints";
                scene_error_active_ = false;
            }

#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
            // Fixtures may drive activation explicitly; background windows do
            // not reliably retain OS key focus across the test process.
            if (!suppress_platform_focus_) ui_.refresh_focus(services_);
#else
            ui_.refresh_focus(services_);
#endif
            preferred_measure_dirty_ = true;
            flush_preferred_size_notification();
            return PUGL_SUCCESS;
        }
        case PUGL_UPDATE:
            nativeuiImeFlushPendingCancel(ime_bridge_);
            flush_preferred_size_notification();
            if (deferred_scene_redraw_) {
                deferred_scene_redraw_ = false;
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
                ++deferred_redraw_attempts_;
                if (std::exchange(reject_deferred_redraw_once_, false)) {
                    ++deferred_redraw_rejections_;
                    return PUGL_SUCCESS;
                }
#endif
                const auto logical = geometry_.logical_size();
                request_redraw(Rect{0.0f, 0.0f, logical.w, logical.h});
            }
            return PUGL_SUCCESS;

        case PUGL_EXPOSE:
            if (!geometry_.renderable()) return PUGL_SUCCESS;
            try {
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
                if (recreate_renderer_on_expose_) {
                    recreate_renderer_on_expose_ = false;
                    renderer_.reset();
                }
                const float render_scale = scene_scale_override_.value_or(
                    geometry_.last_valid_scale());
#else
                const float render_scale = geometry_.last_valid_scale();
#endif
                if (!renderer_.render(ui_,
                                      services_,
                                      geometry_.physical_size(),
                                      render_scale)) {
                    deferred_scene_redraw_ |= renderer_.take_deferred_redraw();
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
                    ++failed_scene_exposes_;
#endif
                    record_scene_error("NativeUI scene build or presentation failed; waiting for a later expose");
                    return PUGL_SUCCESS;
                }
                deferred_scene_redraw_ |= renderer_.take_deferred_redraw();
                clear_scene_error();
            } catch (const std::exception& error) {
                renderer_.invalidate();
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
                ++failed_scene_exposes_;
#endif
                record_scene_error(error.what());
                return PUGL_SUCCESS;
            } catch (...) {
                renderer_.invalidate();
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
                ++failed_scene_exposes_;
#endif
                record_scene_error("Unknown NativeUI scene paint failure");
                return PUGL_SUCCESS;
            }
            return PUGL_SUCCESS;

        case PUGL_CLOSE:
            if (close_callback_) {
                close_callback_();
            } else {
                (void)close_native_view();
            }
            return PUGL_SUCCESS;

        case PUGL_FOCUS_IN:
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
            if (suppress_platform_focus_) return PUGL_SUCCESS;
#endif
            ui_.activate(services_);
            ui_.refresh_focus(services_);
            return PUGL_SUCCESS;

        case PUGL_FOCUS_OUT:
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
            if (suppress_platform_focus_) return PUGL_SUCCESS;
#endif
            clear_raw_pointer_positions();
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
                if (close_callback_) {
                    close_callback_();
                } else {
                    (void)close_native_view();
                }
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

        case PUGL_POINTER_DOWN:
        case PUGL_POINTER_MOVE:
        case PUGL_POINTER_UP:
        case PUGL_POINTER_CANCEL: {
            if (event->pointer.id == PUGL_POINTER_ID_NONE) return PUGL_SUCCESS;

            InputEvent input{};
            input.type = detail::translate_pugl_pointer_event(event->type);
            input.position = detail::physical_to_logical_point(
                {static_cast<float>(event->pointer.x),
                 static_cast<float>(event->pointer.y)},
                geometry_.last_valid_scale());
            input.pointer = detail::translate_pugl_pointer_contact(
                event->pointer, geometry_.last_valid_scale());
            apply_modifiers(input, event->pointer.state);
            update_raw_pointer_position(input);
            (void)ui_.dispatch(input, services_);
            return PUGL_SUCCESS;
        }

        case PUGL_POINTER_IN:
        case PUGL_POINTER_OUT: {
            InputEvent input{};
            input.type = event->type == PUGL_POINTER_IN
                ? InputType::PointerMove
                : InputType::PointerLeave;
            input.position = detail::physical_to_logical_point(
                {static_cast<float>(event->crossing.x),
                 static_cast<float>(event->crossing.y)},
                geometry_.last_valid_scale());
            input.pointer.type = PointerType::Mouse;
            apply_modifiers(input, event->crossing.state);
            last_pointer_position_ = input.position;
            have_pointer_position_ = event->type == PUGL_POINTER_IN;
            (void)ui_.dispatch(input, services_);
            return PUGL_SUCCESS;
        }

        case PUGL_BUTTON_PRESS:
        case PUGL_BUTTON_RELEASE: {
            InputEvent input{};
            input.type = detail::translate_pugl_button(
                event->button.button, event->type == PUGL_BUTTON_PRESS);
            if (input.type == InputType::None) return PUGL_SUCCESS;
            input.position = detail::physical_to_logical_point(
                {static_cast<float>(event->button.x), static_cast<float>(event->button.y)},
                geometry_.last_valid_scale());
            input.pointer.type = PointerType::Mouse;
            apply_modifiers(input, event->button.state);

            if (input.type == InputType::PointerDown) {
                const double now = puglGetTime(world_);
                const float dx = input.position.x - last_click_position_.x;
                const float dy = input.position.y - last_click_position_.y;
                const bool close_in_time = now - last_click_time_ <= kMultiClickSeconds;
                const bool close_in_space = std::sqrt(dx * dx + dy * dy) <= kMultiClickDistance;
                click_count_ = close_in_time && close_in_space ? std::min(3, click_count_ + 1) : 1;
                last_click_time_ = now;
                last_click_position_ = input.position;
            }
            input.clicks = input.type == InputType::ContextMenu ? 1 : click_count_;
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
            input.pointer.type = PointerType::Mouse;
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
            input.pointer.type = PointerType::Mouse;
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

                detail::ScopedBorrowState offer_borrow{
                    active_drop_offer_, drop_offer_decided_, &event->offer};
                (void)ui_.dispatch(input, services_);
                if (!drop_offer_decided_) {
                    (void)puglRejectOffer(view_,
                                          &event->offer,
                                          0,
                                          0,
                                          std::numeric_limits<uint32_t>::max(),
                                          std::numeric_limits<uint32_t>::max());
                }
                return PUGL_SUCCESS;
            }

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

    struct RawPointerPosition {
        PointerId id{};
        Point position{};
    };

    void update_raw_pointer_position(InputEvent& event) noexcept {
        std::size_t index = raw_pointer_position_count_;
        for (std::size_t i = 0; i < raw_pointer_position_count_; ++i) {
            if (raw_pointer_positions_[i].id == event.pointer.id) {
                index = i;
                break;
            }
        }

        const bool terminal =
            event.type == InputType::PointerUp ||
            event.type == InputType::PointerCancel;

        if (index != raw_pointer_position_count_) {
            if (event.type != InputType::PointerDown) {
                event.delta = {
                    event.position.x - raw_pointer_positions_[index].position.x,
                    event.position.y - raw_pointer_positions_[index].position.y};
            }

            if (terminal) {
                --raw_pointer_position_count_;
                if (index != raw_pointer_position_count_) {
                    raw_pointer_positions_[index] =
                        raw_pointer_positions_[raw_pointer_position_count_];
                }
                raw_pointer_positions_[raw_pointer_position_count_] = {};
            } else {
                raw_pointer_positions_[index].position = event.position;
            }
            return;
        }

        if (!terminal && raw_pointer_position_count_ < raw_pointer_positions_.size()) {
            raw_pointer_positions_[raw_pointer_position_count_++] = {
                event.pointer.id, event.position};
        }
    }

    void clear_raw_pointer_positions() noexcept {
        for (std::size_t i = 0; i < raw_pointer_position_count_; ++i) {
            raw_pointer_positions_[i] = {};
        }
        raw_pointer_position_count_ = 0U;
    }

    UI& ui_;
    PlatformServices& services_;
    PuglWorld* world_{};
    bool owns_world_{};
    PuglView* view_{};
    NativeUIImeBridge* ime_bridge_{};
    SkiaGlRenderer renderer_;
    detail::ViewGeometryState geometry_;
    detail::WindowSizeConstraints size_constraints_;
    detail::PreferredSizeState preferred_size_state_;
    PreferredSizeCallback preferred_size_callback_;
    bool preferred_measure_dirty_{};
    bool should_close_{};
    bool visible_{};
    bool deferred_scene_redraw_{};
#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
    std::uint64_t failed_scene_exposes_{};
    std::uint64_t deferred_redraw_attempts_{};
    std::uint64_t deferred_redraw_rejections_{};
    std::uint64_t redraw_requests_during_render_{};
    bool recreate_renderer_on_expose_{};
    std::optional<float> scene_scale_override_;
    bool reject_deferred_redraw_once_{};
    bool suppress_platform_focus_{};
#endif
    bool text_input_active_{};
    Rect text_input_logical_area_{};
    float text_input_logical_cursor_offset_{};
    Rect text_input_physical_area_{};
    float text_input_physical_cursor_offset_{};
    bool have_pointer_position_{};
    Point last_pointer_position_{};
    std::array<RawPointerPosition, 16U> raw_pointer_positions_{};
    std::size_t raw_pointer_position_count_{};
    double last_click_time_{-1000.0};
    Point last_click_position_{};
    int click_count_{};
    const PuglDataOfferEvent* active_drop_offer_{};
    bool drop_offer_decided_{};
    std::string last_error_;
    bool scene_error_active_{};
    std::function<void()> close_callback_;
};

NativeViewConstructionFaultResult exercise_native_view_construction_fault(
    UI& ui,
    PlatformServices& services,
    NativeParentHandle parent,
    Size size,
    NativeViewConstructionFaultStage stage) noexcept {
    NativeViewConstructionFaultResult result{};
    try {
        ViewCore core{
            ui,
            services,
            PUGL_MODULE,
            WindowDesc{
                .title = "NativeUI T130 construction fault probe",
                .size = size,
                .resizable = true,
            },
            parent,
            nullptr,
            {},
            stage,
            &result};
        result.unexpected_success = true;
    } catch (const NativeViewConstructionFault&) {
        result.injected_failure = true;
    } catch (...) {
        result.unexpected_failure = true;
    }
    return result;
}

} // namespace
