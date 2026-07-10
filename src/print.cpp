#include <print.hpp>
#include <uart.hpp>

auto print(const noxx::StringView str) -> void {
    auto span = noxx::Span<const u8>{(const u8*)str.data(), str.size() + 1};
    uart::write_all(span);
}

auto println(const noxx::StringView str) -> void {
    print(str);
    print("\r\n");
}
