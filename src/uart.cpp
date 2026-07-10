#include <hal/uart.hpp>
#include <uart.hpp>

namespace uart {
auto read_all(noxx::Span<u8> buf) -> void {
    while(buf.size > 0) {
        buf = buf.subspan(read(buf));
    }
}

auto write_all(noxx::Span<const u8> buf) -> void {
    while(buf.size > 0) {
        buf = buf.subspan(write(buf));
    }
}
} // namespace uart
