#include <cstring>
#include <print>

#include "noxx/assert.hpp"
#include "noxx/malloc.hpp"
#include "noxx/string-view.hpp"
#include "noxx/string.hpp"

namespace {
#define error_act return false

auto common_test(std::string data) -> bool {
    unwrap(str, noxx::String::create(data.data()));
    ensure(str.size() == data.size());
    ensure(std::strcmp(str.data(), data.data()) == 0);
    str.clear();
    ensure(str.size() == 0);

    ensure(str.resize(data.size() + 5));
    ensure(str.size() == data.size() + 5);
    memcpy(str.data(), data.data(), data.size() + 1);
    ensure(std::strcmp(str.data(), data.data()) == 0);
    memcpy(str.data() + data.size(), "12345", 6);
    ensure(std::strcmp(str.data(), (data + "12345").data()) == 0);
    const auto before_shrink = str.data();
    ensure(str.resize(data.size()));
    ensure(str.data() == before_shrink); // shrink should not change pointer
    ensure(std::strcmp(str.data(), data.data()) == 0);
    ensure(str.append("67890"));
    ensure(std::strcmp(str.data(), (data + "67890").data()) == 0);
    ensure(str.data() == before_shrink); // should not cause reallocation
    return true;
}

auto small_string() -> bool {
    ensure(common_test("short"));
    return true;
}

auto large_string() -> bool {
    ensure(common_test("some long string needs heap allocation, this is large string"));
    return true;
}

#undef error_act

auto heap = std::array<std::byte, 4096>();
} // namespace

auto main() -> int {
#define error_act return -1
    noxx::set_heap(heap.data(), heap.size());
    ensure(small_string());
    ensure(large_string());
    std::println("pass");

    auto str = move(*noxx::String::create("hello "));
    return 0;
}
