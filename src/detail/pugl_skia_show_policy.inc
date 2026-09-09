constexpr auto puglShow = [](PuglView* view, PuglShowCommand command) -> PuglStatus {
    const auto status = ::puglShow(view, command);

    // Pugl documents PUGL_FAILURE from PUGL_SHOW_RAISE as the non-fatal
    // "shown but not raised" outcome. NativeUI requires a live visible window
    // here, not guaranteed foreground activation, so preserve that successful
    // construction while leaving every genuine Pugl error fatal.
    if (command == PUGL_SHOW_RAISE && status == PUGL_FAILURE) {
        return PUGL_SUCCESS;
    }

    return status;
};
