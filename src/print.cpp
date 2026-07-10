#include <coop/promise.hpp>
#include <print.hpp>
#include <uart.hpp>

auto print_blocking(noxx::StringView str) -> void {
    while(str.size() != 0) {
        const auto nl  = str.find("\n");
        const auto run = nl < 0 ? str : str.substr(0, nl);
        if(run.size() != 0) {
            uart::write_blocking({(const u8*)run.data(), run.size()});
        }
        if(nl < 0) {
            break;
        }
        uart::write_blocking({(const u8*)"\r\n", 2});
        str = str.substr(nl + 1);
    }
}

auto print(noxx::StringView str) -> coop::Async<void> {
    while(str.size() != 0) {
        const auto nl  = str.find("\n");
        const auto run = nl < 0 ? str : str.substr(0, nl);
        if(run.size() != 0) {
            co_await uart::write_all({(const u8*)run.data(), run.size()});
        }
        if(nl < 0) {
            break;
        }
        co_await uart::write_all({(const u8*)"\r\n", 2});
        str = str.substr(nl + 1);
    }
}
