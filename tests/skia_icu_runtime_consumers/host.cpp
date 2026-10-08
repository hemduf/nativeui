#include <windows.h>

#include <bit>
#include <exception>
#include <filesystem>
#include <iostream>

namespace {
class LoadedModule {
public:
  explicit LoadedModule(HMODULE value) noexcept : value_(value) {}
  ~LoadedModule() noexcept {
    if (value_)
      FreeLibrary(value_);
  }
  LoadedModule(const LoadedModule &) = delete;
  LoadedModule &operator=(const LoadedModule &) = delete;
  HMODULE get() const noexcept { return value_; }

private:
  HMODULE value_{};
};
} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: runtime_host <Unicode-module-path>\n";
    return 2;
  }
  try {
    const auto module_path = std::filesystem::path{argv[1]}.wstring();
    LoadedModule module{LoadLibraryW(module_path.c_str())};
    if (!module.get()) {
      std::cerr << "Unicode module load failed: " << GetLastError() << '\n';
      return 3;
    }
    const auto symbol = GetProcAddress(module.get(), "nativeui_icu_runtime_probe");
    if (!symbol) {
      std::cerr << "Unicode module export is missing: " << GetLastError() << '\n';
      return 4;
    }
    using Probe = int (*)() noexcept;
    const auto probe = std::bit_cast<Probe>(symbol);
    const int result = probe();
    if (result != 0)
      std::cerr << "Unicode module rejected its module-local ICU data\n";
    return result;
  } catch (const std::exception &error) {
    std::cerr << "Independent Unicode host failed: " << error.what() << '\n';
  } catch (...) {
    std::cerr << "Independent Unicode host failed with an unknown exception\n";
  }
  return 5;
}
