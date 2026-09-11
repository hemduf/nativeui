#include "test_support.hpp"
#include "t067_retained_contract.inc"

namespace {

void suite() {
    t067_retained_contract::behavior_contract();
}

} // namespace

int main() { return test::run("t067_retained", &suite); }
