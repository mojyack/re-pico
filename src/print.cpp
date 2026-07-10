#include <coop/promise.hpp>
#include <print.hpp>
#include <uart.hpp>

auto print_blocking(const noxx::StringView str) -> void {
    auto span = noxx::Span<const u8>{(const u8*)str.data(), str.size() + 1};
    uart::write_blocking(span);
}

auto println_blocking(const noxx::StringView str) -> void {
    print_blocking(str);
    print_blocking("\r\n");
}

auto print(noxx::StringView str) -> coop::Async<void> {
    auto span = noxx::Span<const u8>{(const u8*)str.data(), str.size() + 1};
    co_await uart::write_all(span);
}

auto println(noxx::StringView str) -> coop::Async<void> {
    co_await print(str);
    co_await print("\r\n");
}
