#include <console.hpp>
#include <coop/ext-event.hpp>
#include <coop/promise.hpp>
#include <hal/uart.hpp>
#include <print.hpp>
#include <uart.hpp>

#include <noxx/assert.hpp>

auto read_line() -> coop::Async<noxx::Optional<noxx::String>> {
    constexpr auto error_value = noxx::nullopt;

    auto line = noxx::String();
loop:
    co_ensure(co_await coop::wait_for_event(uart::read_event) == coop::EventResult::Ok);
    auto c = u8();
    co_ensure(uart::read({&c, 1}) == 1);
    switch(c) {
    case '\r':
    case '\n':
        co_await uart::write_all({&c, 1});
        co_return line;
    case '\b':
        if(line.size() > 0) {
            co_await print("\b \b");
            line.resize(line.size() - 1);
        }
        break;
    default:
        co_await uart::write_all({&c, 1});
        line.append(c);
        break;
    }
    goto loop;
}
