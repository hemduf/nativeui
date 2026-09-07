class ViewCore {
public:
    ViewCore(UI& ui,
             PlatformServices& services,
             PuglWorldType world_type,
             WindowDesc desc,
             NativeParentHandle parent)
        : ui_(ui), services_(services), logical_size_(desc.size) {
        world_ = puglNewWorld(world_type, 0);
        if (!world_) throw std::runtime_error("puglNewWorld failed");

        view_ = puglNewView(world_);
        if (!view_) {
            puglFreeWorld(world_);
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

        // NativeUI's public size is logical. Pugl deliberately uses physical
        // pixel geometry, so convert the initial hint at the window boundary.
        scale_factor_ = static_cast<float>(puglGetScaleFactor(view_));
        if (!(scale_factor_ > 0.0f)) scale_factor_ = 1.0f;
        physical_size_ = Size{
            std::max(1.0f, desc.size.w * scale_factor_),
            std::max(1.0f, desc.size.h * scale_factor_)};
        puglSetSizeHint(view_, PUGL_DEFAULT_SIZE,
                        to_pugl_span(physical_size_.w),
                        to_pugl_span(physical_size_.h));
        puglSetViewHint(view_, PUGL_RESIZABLE, desc.resizable ? PUGL_TRUE : PUGL_FALSE);
        // Request a portable modern baseline. On macOS this maps to the native
        // 3.2 Core profile instead of Pugl's legacy 2.0 default.
        puglSetViewHint(view_, PUGL_CONTEXT_VERSION_MAJOR, 3);
        puglSetViewHint(view_, PUGL_CONTEXT_VERSION_MINOR, 2);
        puglSetViewHint(view_, PUGL_CONTEXT_PROFILE, PUGL_OPENGL_CORE_PROFILE);
        puglSetViewHint(view_, PUGL_DOUBLE_BUFFER, PUGL_TRUE);
        puglSetViewHint(view_, PUGL_STENCIL_BITS, 8);
        puglSetViewHint(view_, PUGL_SAMPLES, 0);
        puglSetBackend(view_, puglGlBackend());
        puglSetHandle(view_, this);
        puglSetEventFunc(view_, &ViewCore::event_thunk);
        puglRegisterDropType(view_, "text/plain");
        puglRegisterDropType(view_, "text/uri-list");

        if (const auto status = puglRealize(view_)) {
            cleanup_and_throw(status, "puglRealize failed");
        }

        scale_factor_ = static_cast<float>(puglGetScaleFactor(view_));
        if (!(scale_factor_ > 0.0f)) scale_factor_ = 1.0f;
        ui_.resize(logical_size_);

        if (const auto status = puglShow(view_, parent ? PUGL_SHOW_PASSIVE : PUGL_SHOW_RAISE)) {
            cleanup_and_throw(status, "puglShow failed");
        }

        // Attach exposure scheduling only after the native view is visible.
        // The callback replays the already accumulated initial dirty region,
        // so no redundant whole-view redraw request is needed here.
        ui_.set_invalidation_callback([this](Rect rect) { request_redraw(rect); });
    }

    ~ViewCore() {
        // Release focus/capture/component activation while PlatformServices is
        // still alive. PUGL_FOCUS_OUT normally does this earlier, but teardown
        // must also be deterministic when a host destroys an active view.
        ui_.deactivate(services_);
        set_text_input(false, {}, 0.0f);
        ui_.clear_invalidation_callback();
        if (view_) {
            // PUGL_UNREALIZE is the graphics-safe teardown point. Pugl enters
            // the backend context for this event before destroying the native
            // GL context.
            (void)puglUnrealize(view_);
            puglFreeView(view_);
            view_ = nullptr;
        }
        if (world_) {
            puglFreeWorld(world_);
            world_ = nullptr;
        }
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
        return !should_close_;
    }

    void request_close() { should_close_ = true; }
    [[nodiscard]] bool should_close() const noexcept { return should_close_; }
    [[nodiscard]] Size size() const noexcept { return logical_size_; }
    [[nodiscard]] float scale_factor() const noexcept { return scale_factor_; }
    [[nodiscard]] NativeViewHandle native_handle() const noexcept {
        return view_ ? static_cast<NativeViewHandle>(puglGetNativeView(view_)) : 0;
    }
    [[nodiscard]] const std::string& last_error() const noexcept { return last_error_; }

    bool set_size(Size logical) {
        if (!view_) return false;
        const float scale = static_cast<float>(puglGetScaleFactor(view_));
        const float safe_scale = scale > 0.0f ? scale : 1.0f;
        const auto status = puglSetSizeHint(
            view_,
            PUGL_CURRENT_SIZE,
            to_pugl_span(logical.w * safe_scale),
            to_pugl_span(logical.h * safe_scale));
        return status == PUGL_SUCCESS;
    }

    void request_redraw(Rect logical_rect) {
        if (!view_ || logical_rect.empty()) return;

        const float scale = scale_factor_ > 0.0f ? scale_factor_ : 1.0f;
        const float left = std::floor(logical_rect.x * scale);
        const float top = std::floor(logical_rect.y * scale);
        const float right = std::ceil((logical_rect.x + logical_rect.w) * scale);
        const float bottom = std::ceil((logical_rect.y + logical_rect.h) * scale);

        (void)puglObscureRegion(view_,
                                static_cast<int>(left),
                                static_cast<int>(top),
                                static_cast<unsigned>(std::max(1.0f, right - left)),
                                static_cast<unsigned>(std::max(1.0f, bottom - top)));
    }


    void set_text_input(bool active, Rect, float) {
        if (!view_ || active == text_input_active_) return;
        text_input_active_ = active;
        if (active) {
            puglStartTimer(view_, kCaretTimerId, kCaretBlinkSeconds);
        } else {
            puglStopTimer(view_, kCaretTimerId);
        }
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

            const float scale = scale_factor_ > 0.0f ? scale_factor_ : 1.0f;
            const int x = static_cast<int>(std::floor(logical_region.x * scale));
            const int y = static_cast<int>(std::floor(logical_region.y * scale));
            const unsigned width = static_cast<unsigned>(std::max(1.0f, std::ceil(logical_region.w * scale)));
            const unsigned height = static_cast<unsigned>(std::max(1.0f, std::ceil(logical_region.h * scale)));
            const auto status = puglAcceptOffer(
                view_, active_drop_offer_, i, PUGL_DATA_ACTION_COPY, x, y, width, height);
            drop_offer_decided_ = status == PUGL_SUCCESS;
            return drop_offer_decided_;
        }
        return false;
    }

    void reject_drop(Rect logical_region) {
        if (!view_ || !active_drop_offer_ || active_drop_offer_->clipboard != PUGL_CLIPBOARD_DRAG) return;
        const float scale = scale_factor_ > 0.0f ? scale_factor_ : 1.0f;
        const int x = static_cast<int>(std::floor(logical_region.x * scale));
        const int y = static_cast<int>(std::floor(logical_region.y * scale));
        const unsigned width = static_cast<unsigned>(std::max(1.0f, std::ceil(logical_region.w * scale)));
        const unsigned height = static_cast<unsigned>(std::max(1.0f, std::ceil(logical_region.h * scale)));
        (void)reject_pugl_drop_offer(view_, active_drop_offer_, x, y, width, height);
        drop_offer_decided_ = true;
    }

private:
    void cleanup_and_throw(PuglStatus status, const char* what) {
        if (view_) {
            puglFreeView(view_);
            view_ = nullptr;
        }
        if (world_) {
            puglFreeWorld(world_);
            world_ = nullptr;
        }
        throw_pugl(status, what);
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
            scale_factor_ = static_cast<float>(puglGetScaleFactor(view_));
            if (!(scale_factor_ > 0.0f)) scale_factor_ = 1.0f;
            return PUGL_SUCCESS;