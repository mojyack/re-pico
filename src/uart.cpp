#include <coop/ext-event.hpp>
#include <coop/promise.hpp>
#include <hal/uart.hpp>
#include <uart.hpp>

namespace uart {
auto read_blocking(noxx::Span<u8> buf) -> void {
    while(buf.size() > 0) {
        buf = buf.subspan(read(buf));
    }
}

auto write_blocking(noxx::Span<const u8> buf) -> void {
    while(buf.size() > 0) {
        buf = buf.subspan(write(buf));
    }
}

auto read_all(noxx::Span<u8> buf) -> coop::Async<void> {
    while(buf.size() > 0) {
        co_await coop::wait_for_event(uart::read_event);
        buf = buf.subspan(read(buf));
    }
}

auto write_all(noxx::Span<const u8> buf) -> coop::Async<void> {
    while(buf.size() > 0) {
        co_await coop::wait_for_event(uart::write_event);
        buf = buf.subspan(write(buf));
    }
}
} // namespace uart
