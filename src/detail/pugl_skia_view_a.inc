class ViewCore {
public:
    ViewCore(UI& ui,
             PlatformServices& services,
             PuglWorldType world_type,
             WindowDesc desc,
             NativeParentHandle parent,
             PuglWorld* shared_world = nullptr,
             std::function<void()> close_callback = {})
        : ui_(ui),
          services_(services),
          geometry_(desc.size),
          close_callback_(std::move(close_callback)) {
        if (!detail::valid_logical_size(desc.size)) {
            throw std::invalid_argument("NativeUI initial logical size must be finite and positive");
        }

        if (shared_world) {
            world_ = shared_world;
        } else {
            world_ = puglNewWorld(world_type, 0);
            owns_world_ = true;
        }
        if (!world_) throw std::runtime_error("puglNewWorld failed");

        view_ = puglNewView(world_);
        if (!view_) {
            if (owns_world_) puglFreeWorld(world_);
            world_ = nullptr;
            throw std::runtime_error("puglNewView failed");
        }

        puglSetWorldString(world_, PUGL_CLASS_NAME, "NativeUI");
        puglSetViewString(view_, PUGL_WINDOW_TITLE, desc.title.c_str());

        // Set the parent before querying scale so embedded views resolve the
        // monitor/DPI of the host window rather than a default screen.
        if (parent) {
            const auto status = puglSetParent(view_, static_cast<PuglNativeView>(parent));
            if (status) cleanup_and_throw(status, "puglSetParent failed");
        }

        // NativeUI public geometry is logical. Keep one per-view last-valid
        // scale and convert the initial request exactly once at the platform
        // boundary. Native configure remains authoritative for the actual size.
        record_scale_observation(
            geometry_.observe_scale(static_cast<float>(puglGetScaleFactor(view_))));
        const auto initial_physical = geometry_.physical_request(desc.size);
        if (!initial_physical) {
            cleanup_and_throw(PUGL_BAD_PARAMETER, "invalid initial logical size");
        }
        puglSetSizeHint(view_, PUGL_DEFAULT_SIZE,
                        to_pugl_span(initial_physical->w),
                        to_pugl_span(initial_physical->h));
        puglSetViewHint(view_, PUGL_RESIZABLE, desc.resizable ? PUGL_TRUE : PUGL_FALSE);
        // Request a portable modern baseline. On macOS this maps to the native
        // 3.2 Core profile instead of Pugl's legacy 2.0 default.
        puglSetViewHint(view_, PUGL_CONTEXT_VERSION_MAJOR, 3);
        puglSetViewHint(view_, PUGL_CONTEXT_VERSION_MINOR, 2);
        puglSetViewHint(view_, PUGL_CONTEXT_PROFILE, PUGL_OPENGL_CORE_PROFILE);
        puglSetViewHint(view_, PUGL_DOUBLE_BUFFER, PUGL_TRUE);
        puglSetViewHint(view_, PUGL_STENCIL_BITS, 8);
        puglSetViewHint(view_, PUGL_SAMPLES, 0);
        puglSetViewHint(view_, PUGL_ACCEPT_DROP, PUGL_TRUE);
        puglSetBackend(view_, puglGlBackend());
        puglSetHandle(view_, this);
        puglSetEventFunc(view_, &ViewCore::event_thunk);
        puglRegisterDropType(view_, "text/plain");
        puglRegisterDropType(view_, "text/uri-list");

        if (const auto status = puglRealize(view_)) {
            cleanup_and_throw(status, "puglRealize failed");
        }

        ime_bridge_ = nativeuiImeCreate(world_, view_, this, &ViewCore::ime_event_thunk);
        if (!ime_bridge_) {
            cleanup_and_throw(PUGL_FAILURE, "native IME bridge initialization failed");
        }

        record_scale_observation(
            geometry_.observe_scale(static_cast<float>(puglGetScaleFactor(view_))));
        // Initial layout uses the requested logical size so synchronous focus
        // and IME callbacks during show have valid bounds. Subsequent size
        // changes are driven only by authoritative configure snapshots.
        ui_.resize(geometry_.logical_size());

        if (const auto status = puglShow(view_, parent ? PUGL_SHOW_PASSIVE : PUGL_SHOW_RAISE)) {
            cleanup_and_throw(status, "puglShow failed");
        }

        // Attach exposure scheduling only after the native view is visible.
        // The callback replays the already accumulated initial dirty region,
        // so no redundant whole-view redraw request is needed here.
        ui_.set_invalidation_callback([this](Rect rect) {
            if (ui_.layout_dirty()) preferred_measure_dirty_ = true;
            request_redraw(rect);
        });
    }

    ~ViewCore() {
        // Release focus/capture/component activation while PlatformServices is
        // still alive. PUGL_FOCUS_OUT normally does this earlier, but teardown
        // must also be deterministic when a host destroys an active view.
        ui_.deactivate(services_);
        set_text_input(false, {}, 0.0f);
        preferred_size_callback_ = {};
        preferred_size_state_.reset();
        ui_.clear_invalidation_callback();
        if (ime_bridge_) {
            nativeuiImeDestroy(ime_bridge_);
            ime_bridge_ = nullptr;
        }
        if (view_) {
            // PUGL_UNREALIZE is the graphics-safe teardown point. Pugl enters
            // the backend context for this event before destroying the native
            // GL context.
            (void)puglUnrealize(view_);
            puglFreeView(view_);
            view_ = nullptr;
        }
        if (world_ && owns_world_) {
            puglFreeWorld(world_);
        }
        world_ = nullptr;
    }

    ViewCore(const ViewCore&) = delete;
    ViewCore& operator=(const ViewCore&) = delete;

    bool poll(double timeout) {
        if (!world_ || should_close_) return false;
        const auto status = puglUpdate(world_, timeout);
        if (status && status != PUGL_SUCCESS) {
            last_error_ = puglStrerror(status);
            should_close_ = true;
        }
        if (!should_close_) flush_preferred_size_notification();
        return !should_close_;
    }

    void request_close() { should_close_ = true; }
    [[nodiscard]] bool should_close() const noexcept { return should_close_; }
    [[nodiscard]] Size size() const noexcept { return geometry_.logical_size(); }
    [[nodiscard]] float scale_factor() const noexcept { return geometry_.last_valid_scale(); }
    [[nodiscard]] NativeViewHandle native_handle() const noexcept {
        return view_ ? static_cast<NativeViewHandle>(puglGetNativeView(view_)) : 0;
    }
    [[nodiscard]] const std::string& last_error() const noexcept { return last_error_; }

    bool set_size(Size logical) {
        if (!view_) return false;
        return detail::submit_logical_size_request(
            geometry_,
            logical,
            [this](Size physical) {
                return puglSetSizeHint(
                           view_,
                           PUGL_CURRENT_SIZE,
                           to_pugl_span(physical.w),
                           to_pugl_span(physical.h)) == PUGL_SUCCESS;
            });
    }

    void set_preferred_size_callback(PreferredSizeCallback callback) {
        preferred_size_callback_ = std::move(callback);
        preferred_size_state_.reset();
        preferred_measure_dirty_ = static_cast<bool>(preferred_size_callback_);
        flush_preferred_size_notification();
    }

    void request_redraw(Rect logical_rect) {
        if (!view_ || logical_rect.empty()) return;

        const auto physical = detail::logical_to_physical_covering_rect(
            logical_rect, geometry_.last_valid_scale());
        if (physical.w <= 0.0f || physical.h <= 0.0f) return;

        (void)puglObscureRegion(view_,
                                static_cast<int>(physical.x),
                                static_cast<int>(physical.y),
                                static_cast<unsigned>(physical.w),
                                static_cast<unsigned>(physical.h));
    }

    void set_text_input(bool active, Rect logical_area, float logical_cursor_offset) {
        if (!view_ || !ime_bridge_) return;

        const float scale = geometry_.last_valid_scale();
        if (!detail::text_input_boundary_needs_update(
                text_input_active_,
                text_input_physical_area_,
                text_input_physical_cursor_offset_,
                active,
                logical_area,
                logical_cursor_offset,
                scale)) {
            return;
        }

        const bool active_changed = active != text_input_active_;
        text_input_active_ = active;
        text_input_logical_area_ = active ? logical_area : Rect{};
        text_input_logical_cursor_offset_ = active ? logical_cursor_offset : 0.0f;

        const auto scaled = detail::scale_text_input_geometry(
            text_input_logical_area_, text_input_logical_cursor_offset_, scale);
        text_input_physical_area_ = scaled.first;
        text_input_physical_cursor_offset_ = scaled.second;

        if (active_changed) {
            if (active) {
                (void)puglStartTimer(view_, kCaretTimerId, kCaretBlinkSeconds);
            } else {
                (void)puglStopTimer(view_, kCaretTimerId);
            }
        }

        nativeuiImeUpdate(ime_bridge_,
                          active,
                          text_input_physical_area_.x,
                          text_input_physical_area_.y,
                          text_input_physical_area_.w,
                          text_input_physical_area_.h,
                          text_input_physical_cursor_offset_);
    }

    void set_clipboard_text(std::string_view text) {
        if (!view_) return;

        // Pugl's macOS backend maps the canonical text/plain MIME type to
        // public.utf8-plain-text. Parameterized MIME strings can resolve to a
        // nil UTI and crash inside NSPasteboard on recent macOS releases.
        puglSetClipboard(view_,
                         PUGL_CLIPBOARD_GENERAL,
                         "text/plain",
                         text.data(),
                         text.size());
    }

    void request_clipboard_text() {
        if (view_) puglPaste(view_);
    }

    bool accept_drop(std::string_view type, Rect logical_region) {
        if (!view_ || !active_drop_offer_ || active_drop_offer_->clipboard != PUGL_CLIPBOARD_DRAG) {
            return false;
        }

        const auto count = puglGetNumClipboardTypes(view_, PUGL_CLIPBOARD_DRAG);
        for (uint32_t i = 0; i < count; ++i) {
            const char* offered = puglGetClipboardType(view_, PUGL_CLIPBOARD_DRAG, i);
            if (!offered || type != offered) continue;

            const auto physical = detail::logical_to_physical_covering_rect(
                logical_region, geometry_.last_valid_scale());
            const auto status = puglAcceptOffer(
                view_, active_drop_offer_, i, PUGL_DATA_ACTION_COPY,
                static_cast<int>(physical.x),
                static_cast<int>(physical.y),
                static_cast<unsigned>(std::max(1.0f, physical.w)),
                static_cast<unsigned>(std::max(1.0f, physical.h)));
            drop_offer_decided_ = status == PUGL_SUCCESS;
            return drop_offer_decided_;
        }
        return false;
    }

    void reject_drop(Rect logical_region) {
        if (!view_ || !active_drop_offer_ || active_drop_offer_->clipboard != PUGL_CLIPBOARD_DRAG) return;
        const auto physical = detail::logical_to_physical_covering_rect(
            logical_region, geometry_.last_valid_scale());
        (void)puglRejectOffer(view_, active_drop_offer_,
                              static_cast<int>(physical.x),
                              static_cast<int>(physical.y),
                              static_cast<unsigned>(std::max(1.0f, physical.w)),
                              static_cast<unsigned>(std::max(1.0f, physical.h)));
        drop_offer_decided_ = true;
    }

private:
    void record_scale_observation(bool accepted) {
        if (!accepted) {
            last_error_ = "NativeUI ignored invalid platform scale factor";
        }
    }

    void queue_preferred_size() {
        if (!preferred_size_callback_) return;
        preferred_size_state_.queue(ui_.measure().preferred);
    }

    void flush_preferred_size_notification() {
        if (!preferred_size_callback_) return;
        if (preferred_measure_dirty_) {
            preferred_measure_dirty_ = false;
            queue_preferred_size();
        }
        (void)preferred_size_state_.dispatch_once([this](Size preferred) {
            auto callback = preferred_size_callback_;
            if (callback) callback(preferred);
        });
    }

    void cleanup_and_throw(PuglStatus status, const char* what) {
        if (ime_bridge_) {
            nativeuiImeDestroy(ime_bridge_);
            ime_bridge_ = nullptr;
        }
        if (view_) {
            puglFreeView(view_);
            view_ = nullptr;
        }
        if (world_ && owns_world_) {
            puglFreeWorld(world_);
        }
        world_ = nullptr;
        throw_pugl(status, what);
    }

    static void ime_event_thunk(void* user_data,
                                NativeUIImeEventType type,
                                const char* utf8,
                                size_t utf8_size,
                                size_t cursor_byte,
                                size_t selection_bytes) noexcept {
        auto* self = static_cast<ViewCore*>(user_data);
        if (!self) return;
        try {
            InputEvent input{};
            input.type = InputType::Composition;
            switch (type) {
                case NATIVEUI_IME_START: input.composition.type = CompositionType::Start; break;
                case NATIVEUI_IME_UPDATE: input.composition.type = CompositionType::Update; break;
                case NATIVEUI_IME_COMMIT: input.composition.type = CompositionType::Commit; break;
                case NATIVEUI_IME_CANCEL: input.composition.type = CompositionType::Cancel; break;
            }
            if (utf8 && utf8_size) input.composition.text.assign(utf8, utf8_size);
            input.composition.cursor_byte = cursor_byte;
            input.composition.selection_bytes = selection_bytes;
            (void)self->ui_.dispatch(input, self->services_);
        } catch (...) {
            // Never unwind through Cocoa, Win32, or XIM callbacks.
            self->should_close_ = true;
        }
    }

    static PuglStatus event_thunk(PuglView* view, const PuglEvent* event) noexcept {
        auto* self = static_cast<ViewCore*>(puglGetHandle(view));
        if (!self) return PUGL_BAD_PARAMETER;
        try {
            return self->on_event(event);
        } catch (const std::exception& e) {
            self->last_error_ = e.what();
            self->should_close_ = true;
            return PUGL_UNKNOWN_ERROR;
        } catch (...) {
            self->last_error_ = "Unknown NativeUI event error";
            self->should_close_ = true;
            return PUGL_UNKNOWN_ERROR;
        }
    }

    PuglStatus on_event(const PuglEvent* event) {
        switch (event->type) {
        case PUGL_REALIZE:
            record_scale_observation(
                geometry_.observe_scale(static_cast<float>(puglGetScaleFactor(view_))));
            return PUGL_SUCCESS;