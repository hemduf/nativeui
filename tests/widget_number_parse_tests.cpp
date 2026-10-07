#include "../src/detail/widget_number_parse.hpp"
#include "test_support.hpp"

#include <cstdlib>
#include <limits>
#include <locale>
#include <new>

// This fault belongs only to this isolated test executable. Production parsing
// owns its stream and never changes a process-global locale or allocation hook.
namespace number_parse_allocation_fault {
bool armed{};
unsigned hits{};
void *allocate(std::size_t size) {
  if (std::exchange(armed, false)) {
    ++hits;
    throw std::bad_alloc{};
  }
  if (void *memory = std::malloc(size ? size : 1))
    return memory;
  throw std::bad_alloc{};
}
} // namespace number_parse_allocation_fault
void *operator new(std::size_t size) {
  return number_parse_allocation_fault::allocate(size);
}
void *operator new[](std::size_t size) {
  return number_parse_allocation_fault::allocate(size);
}
void operator delete(void *memory) noexcept { std::free(memory); }
void operator delete[](void *memory) noexcept { std::free(memory); }
void operator delete(void *memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void *memory, std::size_t) noexcept { std::free(memory); }

namespace {
void expect_value(std::string_view text, double expected) {
  const auto parsed = ui::detail::parse_decimal_number(text);
  NUI_CHECK(parsed && *parsed == expected);
}
void finite_boundaries_and_signed_zero() {
  const std::pair<std::string_view, double> cases[] = {
      {"1.", 1.0},
      {".5", 0.5},
      {"+1.25e1", 12.5},
      {"1.7976931348623157e308", std::numeric_limits<double>::max()},
      {"4.9406564584124654e-324", std::numeric_limits<double>::denorm_min()},
      {"3e-324", std::numeric_limits<double>::denorm_min()},
      {"2.225073858507201e-308",
       std::nextafter(std::numeric_limits<double>::min(), 0.0)},
      {"2.2250738585072012e-308", std::numeric_limits<double>::min()}};
  for (const auto &[text, expected] : cases) {
    expect_value(text, expected);
    if (text.front() != '+')
      expect_value("-" + std::string{text}, -expected);
  }
  for (const auto text : {"0", "+0.0e99999", "0e-99999"}) {
    const auto parsed = ui::detail::parse_decimal_number(text);
    NUI_CHECK(parsed && *parsed == 0.0 && !std::signbit(*parsed));
  }
  for (const auto text : {"-0", "-0.00e99999", "-0e-99999"}) {
    const auto parsed = ui::detail::parse_decimal_number(text);
    NUI_CHECK(parsed && *parsed == 0.0 && std::signbit(*parsed));
  }
}
void malformed_and_unrepresentable_values_reject() {
  for (const auto text : {"", "+", "-", ".", "1e", "1e+", "1e-", "++1",
                          "+-1", "--1", "1.2.3", "1,25", "0x1p0", "nan",
                          "inf", "1.0x", " 1", "1 ", "2e-324", "-2e-324",
                          "1e-9999", "-1e-9999", "1e9999", "-1e9999",
                          "1.7976931348623159e308", "-1.7976931348623159e308",
                          "0e+", "0x0"})
    NUI_CHECK(!ui::detail::parse_decimal_number(text));
  const char embedded_null[] = {'1', '\0', '2'};
  NUI_CHECK(!ui::detail::parse_decimal_number(
      std::string_view{embedded_null, sizeof(embedded_null)}));
  expect_value("1.25", 1.25);
}
struct CommaPunctuation final : std::numpunct<char> {
  char do_decimal_point() const override { return ','; }
  char do_thousands_sep() const override { return '.'; }
  std::string do_grouping() const override { return "\3"; }
};
void host_locale_does_not_change_ascii_parsing() {
  const std::locale before;
  const std::locale comma{std::locale::classic(), new CommaPunctuation};
  bool restoration_failed{};
  struct RestoreLocale {
    std::locale previous;
    bool &failed;
    ~RestoreLocale() noexcept {
      try {
        std::locale::global(previous);
      } catch (...) {
        failed = true;
      }
    }
  };
  {
    RestoreLocale restore{std::locale::global(comma), restoration_failed};
    expect_value("1.25", 1.25);
    expect_value("1.234", 1.234);
    NUI_CHECK(!ui::detail::parse_decimal_number("1,25"));
    NUI_CHECK(std::locale{} == comma);
  }
  NUI_CHECK(!restoration_failed && std::locale{} == before);
  expect_value("2.5", 2.5);
}
void allocation_failure_rejects_and_next_parse_recovers() {
  const std::string text = std::string(256, '0') + "1.25";
  number_parse_allocation_fault::hits = 0;
  number_parse_allocation_fault::armed = true;
  const auto rejected = ui::detail::parse_decimal_number(text);
  const bool still_armed =
      std::exchange(number_parse_allocation_fault::armed, false);
  NUI_CHECK(!rejected && !still_armed && number_parse_allocation_fault::hits == 1);
  expect_value(text, 1.25);
  expect_value("3.5", 3.5);
}
void suite() {
  finite_boundaries_and_signed_zero();
  malformed_and_unrepresentable_values_reject();
  host_locale_does_not_change_ascii_parsing();
  allocation_failure_rejects_and_next_parse_recovers();
}
} // namespace
int main() { return test::run("widget_number_parse", suite); }
