#include "unicode_probe.hpp"

// Never unwind through the host's foreign export boundary, including failures
// from Unicode initialization, retained layout or headless raster allocation.
extern "C" __declspec(dllexport) int nativeui_icu_runtime_probe() noexcept {
  try {
    run_unicode_probe();
    return 0;
  } catch (...) {
    return 1;
  }
}
