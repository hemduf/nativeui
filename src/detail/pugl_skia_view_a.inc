enum class NativeViewConstructionFaultStage : std::uint8_t {
    AfterViewCreation,
    AfterRealize,
    AfterImeCreate,
    AfterSizeConstraints,
    AfterInitialResize,
    AfterShow,
    AfterInvalidationCallback,
};

struct NativeViewConstructionFaultResult {
    bool injected_failure{};
    bool unexpected_failure{};
    bool unexpected_success{};
    std::uint32_t world_acquired{};
    std::uint32_t world_released{};
    std::uint32_t view_acquired{};
    std::uint32_t view_released{};
    std::uint32_t realize_succeeded{};
    std::uint32_t unrealize_released{};
    std::uint32_t ime_acquired{};
    std::uint32_t ime_released{};
    std::uint32_t size_constraints_applied{};
    std::uint32_t initial_resize_completed{};
    std::uint32_t show_completed{};
    std::uint32_t invalidation_attached{};
    std::uint32_t invalidation_cleared{};
};

class NativeViewConstructionFault final {};

class ViewCore {
public:
    ViewCore(UI& ui,
             PlatformServices& services,
             PuglWorldType world_type,
             WindowDesc desc,
             NativeParentHandle parent,
             PuglWorld* shared_world = nullptr,
             std::function<void()> close_callback = {},
             NativeViewConstructionFaultStage fault_stage = NativeViewConstructionFaultStage::AfterViewCreation,
             NativeViewConstructionFaultResult* fault_result = nullptr)
        : ui_(ui),
          services_(services),
          geometry_(validated_initial_size(desc)),
          close_callback_(std::move(close_callback)) {
        if (!size_constraints_.update(desc.min_size, desc.max_size)) {
            throw std::invalid_argument("NativeUI window min/max size constraints are invalid");
        }

        ConstructionCleanup construction_cleanup{*this, fault_result};
        const auto maybe_inject = [&](NativeViewConstructionFaultStage stage) {
            if (fault_result && fault_stage == stage) throw NativeViewConstructionFault{};
        };

        if (shared_world) {
            world_ = shared_world;
        } else {
            world_ = puglNewWorld(world_type, 0);
            owns_world_ = true;
        }
        if (!world_) throw std::runtime_error("puglNewWorld failed");
        if (owns_world_) {
            construction_cleanup.world_acquired = true;
            if (fault_result) ++fault_result->world_acquired;
        }

        view_ = puglNewView(world_);
        if (!view_) {
            throw std::runtime_error("puglNewView failed");
        }
        construction_cleanup.view_acquired = true;
        if (fault_result) ++fault_result->view_acquired;
        maybe_inject(NativeViewConstructionFaultStage::AfterViewCreation);

        puglSetWorldString(world_, PUGL_CLASS_NAME, "NativeUI");
        puglSetViewString(view_, PUGL_WINDOW_TITLE, desc.title.c_str());

        // Set the parent before querying scale so embedded views resolve the
        // monitor/DPI of the host window rather than a default screen.
        if (parent) {
            const auto status = puglSetParent(view_, static_cast<PuglNativeView>(parent));
            if (status) throw_pugl(status, "puglSetParent failed");
        }

        // NativeUI public geometry is logical. Keep one per-view last-valid
        // scale and convert the initial request exactly once at the platform
        // boundary. Native configure remains authoritative for the actual size.
        record_scale_observation(
            geometry_.observe_scale(static_cast<float>(puglGetScaleFactor(view_))));
        const auto initial_physical = geometry_.physical_request(geometry_.logical_size());
        if (!initial_physical) {
            throw_pugl(PUGL_BAD_PARAMETER, "invalid initial logical size");
        }
        puglSetSizeHint(view_, PUGL_DEFAULT_SIZE,
                        to_pugl_span(initial_physical->w),
                        to_pugl_span(initial_physical->h));
        puglSetViewHint(view_, PUGL_RESIZABLE, desc.resizable ? PUGL_TRUE : PUGL_FALSE);
#if defined(__EMSCRIPTEN__)
        // Pugl's browser backend maps a 3.0 GLES request to WebGL2, which is
        // what Skia's WebGL interface requires. Desktop core profiles and
        // minor versions do not exist in the browser.
        puglSetViewHint(view_, PUGL_CONTEXT_API, PUGL_OPENGL_ES_API);
        puglSetViewHint(view_, PUGL_CONTEXT_VERSION_MAJOR, 3);
        puglSetViewHint(view_, PUGL_CONTEXT_VERSION_MINOR, 0);
#else
        // Request a portable modern baseline. On macOS this maps to the native
        // 3.2 Core profile instead of Pugl's legacy 2.0 default.
        puglSetViewHint(view_, PUGL_CONTEXT_VERSION_MAJOR, 3);
        puglSetViewHint(view_, PUGL_CONTEXT_VERSION_MINOR, 2);
        puglSetViewHint(view_, PUGL_CONTEXT_PROFILE, PUGL_OPENGL_CORE_PROFILE);
#endif
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
            throw_pugl(status, "puglRealize failed");
        }
        construction_cleanup.realized = true;
        if (fault_result) ++fault_result->realize_succeeded;
        maybe_inject(NativeViewConstructionFaultStage::AfterRealize);

        ime_bridge_ = nativeuiImeCreate(world_, view_, this, &ViewCore::ime_event_thunk);
        if (!ime_bridge_) {
            throw_pugl(PUGL_FAILURE, "native IME bridge initialization failed");
        }
        construction_cleanup.ime_acquired = true;
        if (fault_result) ++fault_result->ime_acquired;
        maybe_inject(NativeViewConstructionFaultStage::AfterImeCreate);

        record_scale_observation(
            geometry_.observe_scale(static_cast<float>(puglGetScaleFactor(view_))));
        // Apply logical min/max only after realization. In particular, Pugl's
        // macOS realize path builds wrapper-view Auto Layout constraints directly
        // from pre-realize size hints even though those hints are physical pixels.
        // Deferring the hints until the native view exists makes Pugl route them
        // through the platform runtime setters, which convert them to Cocoa points
        // and avoids a conflicting backing-size constraint during visible resize.
        if (!apply_size_constraints()) {
            throw_pugl(PUGL_BAD_PARAMETER, "failed to apply window size constraints");
        }
        if (fault_result) ++fault_result->size_constraints_applied;
        maybe_inject(NativeViewConstructionFaultStage::AfterSizeConstraints);

        // Initial layout uses the requested logical size so synchronous focus
        // and IME callbacks during show have valid bounds. Subsequent size
        // changes are driven only by authoritative configure snapshots.
        ui_.resize(geometry_.logical_size());
        construction_cleanup.retained_started = true;
        if (fault_result) ++fault_result->initial_resize_completed;
        maybe_inject(NativeViewConstructionFaultStage::AfterInitialResize);

        if (const auto status = puglShow(view_, parent ? PUGL_SHOW_PASSIVE : PUGL_SHOW_RAISE)) {
            throw_pugl(status, "puglShow failed");
        }
        visible_ = true;
        if (fault_result) ++fault_result->show_completed;
        maybe_inject(NativeViewConstructionFaultStage::AfterShow);

        // Attach exposure scheduling only after the native view is visible.
        // The callback replays the already accumulated initial dirty region,
        // so no redundant whole-view redraw request is needed here.
        construction_cleanup.invalidation_attached = true;
        ui_.set_invalidation_callback([this](Rect rect) {
            if (ui_.layout_dirty()) preferred_measure_dirty_ = true;
            request_redraw(rect);
        });
        if (fault_result) ++fault_result->invalidation_attached;
        maybe_inject(NativeViewConstructionFaultStage::AfterInvalidationCallback);
        construction_cleanup.release();
    }

    ~ViewCore() noexcept {
        (void)close_native_view();
        release_owned_world_noexcept();
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

    void request_close() { (void)close_native_view(); }
    [[nodiscard]] bool should_close() const noexcept { return should_close_; }
    [[nodiscard]] bool native_view_open() const noexcept { return view_ != nullptr && !should_close_; }
    [[nodiscard]] Size size() const noexcept { return geometry_.logical_size(); }
    [[nodiscard]] float scale_factor() const noexcept { return geometry_.last_valid_scale(); }
    [[nodiscard]] NativeViewHandle native_handle() const noexcept {
        return view_ ? static_cast<NativeViewHandle>(puglGetNativeView(view_)) : 0;
    }

#if defined(NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS)
    [[nodiscard]] bool request_gpu_readback(Point logical_point) noexcept {
        if (!view_ ||
            !std::isfinite(logical_point.x) ||
            !std::isfinite(logical_point.y)) {
            return false;
        }
        const float scale = geometry_.last_valid_scale();
        const auto logical = geometry_.logical_size();
        const float x_logical = std::clamp(logical_point.x, 0.0f, logical.w);
        const float y_logical = std::clamp(logical_point.y, 0.0f, logical.h);
        const int x = static_cast<int>(std::lround(x_logical * scale));
        const int y = static_cast<int>(std::lround(y_logical * scale));
        renderer_.request_readback(x, y);
        request_redraw(Rect{0.0f, 0.0f, logical.w, logical.h});
        return true;
    }

    [[nodiscard]] std::optional<PlatformReadbackPixel>
    take_gpu_readback() noexcept {
        return renderer_.take_readback();
    }
#endif
    [[nodiscard]] const std::string& last_error() const noexcept { return last_error_; }

    bool set_title(std::string_view title) {
        if (!native_view_open()) return false;
        const std::string owned{title};
        return puglSetViewString(view_, PUGL_WINDOW_TITLE, owned.c_str()) == PUGL_SUCCESS;
    }

    bool show() {
        if (!native_view_open()) return false;
        if (visible_) return true;
        if (puglShow(view_, PUGL_SHOW_RAISE) != PUGL_SUCCESS) return false;
        visible_ = true;
        return true;
    }

    bool hide() {
        if (!native_view_open()) return false;
        if (!visible_) return true;
        if (puglHide(view_) != PUGL_SUCCESS) return false;
        visible_ = false;
        return true;
    }

    bool set_size(Size logical) {
        if (!native_view_open()) return false;
        const auto clamped = size_constraints_.clamp(logical);
        if (!clamped) return false;
        return detail::submit_logical_size_request(
            geometry_,
            *clamped,
            [this](Size physical) {
                return puglSetSizeHint(
                           view_,
                           PUGL_CURRENT_SIZE,
                           to_pugl_span(physical.w),
                           to_pugl_span(physical.h)) == PUGL_SUCCESS;
            });
    }

    bool set_min_size(std::optional<Size> logical) {
        return update_size_constraints(logical, size_constraints_.max_size());
    }

    bool set_max_size(std::optional<Size> logical) {
        return update_size_constraints(size_constraints_.min_size(), logical);
    }

    bool close_native_view() noexcept {
        if (!view_) {
            should_close_ = true;
            visible_ = false;
            return false;
        }

        const auto best_effort = [this](auto&& step, const char* fallback) noexcept {
            try {
                step();
            } catch (const std::exception& error) {
                remember_teardown_error(error.what());
            } catch (...) {
                remember_teardown_error(fallback);
            }
        };

        // Release focus/capture/component activation while PlatformServices is
        // still alive. A component fault must not abort native resource release
        // or the destruction of an unrelated sibling window.
        best_effort([this] { (void)ui_.cancel_pointer(services_); },
                    "NativeUI pointer teardown failed");
        best_effort([this] { ui_.deactivate(services_); },
                    "NativeUI component deactivation failed");
        best_effort([this] { set_text_input(false, {}, 0.0f); },
                    "NativeUI text-input teardown failed");
        best_effort([this] { preferred_size_callback_ = {}; },
                    "NativeUI preferred-size callback teardown failed");
        best_effort([this] { preferred_size_state_.reset(); },
                    "NativeUI preferred-size state teardown failed");
        best_effort([this] { ui_.clear_invalidation_callback(); },
                    "NativeUI invalidation callback teardown failed");
        if (ime_bridge_) {
            nativeuiImeDestroy(ime_bridge_);
            ime_bridge_ = nullptr;
        }

        // PUGL_UNREALIZE is the graphics-safe teardown point. Pugl enters the
        // backend context for this event before destroying the native GL context.
        (void)puglUnrealize(view_);
        puglFreeView(view_);
        view_ = nullptr;
        visible_ = false;
        should_close_ = true;
        return true;
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
    struct ConstructionCleanup {
        explicit ConstructionCleanup(ViewCore& owner,
                                     NativeViewConstructionFaultResult* fault_result) noexcept
            : owner_(&owner), fault_result(fault_result) {}
        ~ConstructionCleanup() noexcept {
            if (armed_) owner_->cleanup_partial_construction_noexcept(*this);
        }
        void release() noexcept { armed_ = false; }

        ViewCore* owner_{};
        NativeViewConstructionFaultResult* fault_result{};
        bool armed_{true};
        bool world_acquired{};
        bool view_acquired{};
        bool realized{};
        bool ime_acquired{};
        bool retained_started{};
        bool invalidation_attached{};
    };

    [[nodiscard]] static Size validated_initial_size(const WindowDesc& desc) {
        detail::WindowSizeConstraints constraints;
        if (!constraints.update(desc.min_size, desc.max_size)) {
            throw std::invalid_argument("NativeUI window min/max size constraints are invalid");
        }
        const auto clamped = constraints.clamp(desc.size);
        if (!clamped) {
            throw std::invalid_argument("NativeUI initial logical size must be finite and positive");
        }
        return *clamped;
    }

    [[nodiscard]] bool apply_size_constraints() {
        if (!view_) return false;
        const float scale = geometry_.last_valid_scale();

        const auto min_logical = size_constraints_.min_size();
        const auto max_logical = size_constraints_.max_size();
        const Size min_physical = min_logical
            ? detail::logical_to_physical_size(*min_logical, scale)
            : Size{1.0f, 1.0f};
        const Size max_physical = max_logical
            ? detail::logical_to_physical_size(*max_logical, scale)
            : Size{static_cast<float>(INT16_MAX), static_cast<float>(INT16_MAX)};

        const auto min_status = puglSetSizeHint(
            view_, PUGL_MIN_SIZE, to_pugl_span(min_physical.w), to_pugl_span(min_physical.h));
        if (min_status != PUGL_SUCCESS) return false;
        const auto max_status = puglSetSizeHint(
            view_, PUGL_MAX_SIZE, to_pugl_span(max_physical.w), to_pugl_span(max_physical.h));
        return max_status == PUGL_SUCCESS;
    }

    [[nodiscard]] bool update_size_constraints(std::optional<Size> min_size,
                                               std::optional<Size> max_size) {
        if (!native_view_open()) return false;
        const auto previous = size_constraints_;
        if (!size_constraints_.update(min_size, max_size)) return false;
        if (!apply_size_constraints()) {
            size_constraints_ = previous;
            (void)apply_size_constraints();
            return false;
        }

        const auto clamped = size_constraints_.clamp(geometry_.logical_size());
        if (!clamped) return false;
        if (clamped->w == geometry_.logical_size().w &&
            clamped->h == geometry_.logical_size().h) {
            return true;
        }
        return set_size(*clamped);
    }

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

    void remember_teardown_error(const char* message) noexcept {
        if (!last_error_.empty()) return;
        try {
            last_error_ = message ? message : "NativeUI teardown failed";
        } catch (...) {
            // Teardown diagnostics must never compromise native cleanup.
        }
    }

    void release_owned_world_noexcept() noexcept {
        if (world_ && owns_world_) {
            puglFreeWorld(world_);
        }
        world_ = nullptr;
    }

    void cleanup_partial_construction_noexcept(ConstructionCleanup& cleanup) noexcept {
        const auto best_effort = [](auto&& step) noexcept {
            try {
                step();
            } catch (...) {
                // Preserve the original constructor exception while continuing
                // every remaining retained/native cleanup stage.
            }
        };

        // A successful initial resize may be followed by synchronous focus or
        // activation during show. Contain application teardown faults so a
        // constructor failure still releases all native resources.
        if (cleanup.retained_started) {
            best_effort([this] { (void)ui_.cancel_pointer(services_); });
            best_effort([this] { ui_.deactivate(services_); });
            cleanup.retained_started = false;
        }
        if (cleanup.invalidation_attached) {
            best_effort([this] { ui_.clear_invalidation_callback(); });
            cleanup.invalidation_attached = false;
            if (cleanup.fault_result) ++cleanup.fault_result->invalidation_cleared;
        }
        if (ime_bridge_) {
            nativeuiImeDestroy(ime_bridge_);
            ime_bridge_ = nullptr;
            cleanup.ime_acquired = false;
            if (cleanup.fault_result) ++cleanup.fault_result->ime_released;
        }
        if (view_) {
            if (cleanup.realized) {
                (void)puglUnrealize(view_);
                cleanup.realized = false;
                if (cleanup.fault_result) ++cleanup.fault_result->unrealize_released;
            }
            puglFreeView(view_);
            view_ = nullptr;
            cleanup.view_acquired = false;
            if (cleanup.fault_result) ++cleanup.fault_result->view_released;
        }
        visible_ = false;
        if (world_ && owns_world_) {
            puglFreeWorld(world_);
            world_ = nullptr;
            cleanup.world_acquired = false;
            if (cleanup.fault_result) ++cleanup.fault_result->world_released;
        } else if (owns_world_) {
            world_ = nullptr;
        }
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
            self->remember_teardown_error(e.what());
            self->should_close_ = true;
            return PUGL_UNKNOWN_ERROR;
        } catch (...) {
            self->remember_teardown_error("Unknown NativeUI event error");
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
