#if defined(_WIN32)
#define ICU_FIXTURE_EXPORT __declspec(dllexport)
#else
#define ICU_FIXTURE_EXPORT __attribute__((visibility("default")))
#endif

extern "C" ICU_FIXTURE_EXPORT int nativeui_icu_staging_stub() noexcept { return 0; }
