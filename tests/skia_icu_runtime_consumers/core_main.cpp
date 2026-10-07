#include "unicode_probe.hpp"

#include <exception>
#include <iostream>

int main() {
  try {
    run_unicode_probe();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Windows Core Unicode fixture failed: " << error.what() << '\n';
  } catch (...) {
    std::cerr << "Windows Core Unicode fixture failed with an unknown exception\n";
  }
  return 1;
}
