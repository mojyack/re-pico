// MM8108 HaLow radio driver
// the chip speaks the SD-over-SPI protocol (CMD52/CMD53 against a 64KB address
// window) on SPI2, plus a few sideband GPIOs

#include <coop/promise.hpp>
#include <coop/timer.hpp>
#include <hal/spi.hpp>
#include <hal/time.hpp>
#include <noxx/array.hpp>

#include "crc.hpp"
#include "halow.hpp"
#include "platform.hpp"

#include <noxx/assert.hpp>

namespace halow {
namespace {
constexpr auto max_bus_attempts = u32(200);

// sdio command indices
struct Cmd {
    enum : u8 {
        GoIdle       = 0,  // reset card state
        RWDirect     = 52, // single-byte register access
        RWExtended   = 53, // multi-byte data transfer
        MorseStartup = 63, // vendor command, enters sdio-over-spi operation
    };
};

// sd-over-spi command/response tokens
constexpr auto tkn_start_block   = u8(0xFE); // read data / single block write
constexpr auto tkn_data_accepted = u8(0xE5);

// CMD52/CMD53 argument encoding
constexpr auto arg_write      = u32(1) << 31;
constexpr auto arg_func1      = u32(1) << 28;
constexpr auto arg_func2      = u32(2) << 28;
constexpr auto arg_block_mode = u32(1) << 27; // count field counts blocks, not bytes
constexpr auto arg_addr_inc   = u32(1) << 26;
constexpr auto arg_addr_bits  = u32(9);

// sd-over-spi block sizes (function 1 = registers, function 2 = memory)
constexpr auto block_size_func1 = u32(8);
constexpr auto block_size_func2 = u32(512);
constexpr auto max_blocks       = u32(128); // CMD53 block-count limit

// multi-block write start/stop tokens (single-block/read uses tkn_start_block)
constexpr auto tkn_multi_block = u8(0xFC);
constexpr auto tkn_stop_tran   = u8(0xFD);

// registers reachable via CMD52 (function shared with the window they configure)
constexpr auto reg_window_low    = u32(0x10000); // address[23:16]
constexpr auto reg_window_high   = u32(0x10001); // address[31:24]
constexpr auto reg_window_access = u32(0x10002); // 0=1byte 1=2byte 2=4byte
constexpr auto access_4byte      = u8(2);

// function 2 covers chip memory, function 1 everything else (mm8108 memory map)
struct MemRange {
    u32 begin;
    u32 end;
};
constexpr MemRange memory_ranges[] = {
    {0x00170000, 0x00180000}, // host yaps
    {0x00100000, 0x00120000}, // apps dmem
    {0x00120000, 0x00130000}, // mac dmem
    {0x00200000, 0x002a8000}, // membank imem
    {0x00500000, 0x00520000}, // uphy mem
    {0x00538000, 0x0053C000}, // uphy aon
    {0x00540000, 0x00550000}, // lphy mem
    {0x0055c000, 0x0055e000}, // lphy aon
};

auto is_memory(const u32 address) -> bool {
    for(const auto& range : memory_ranges) {
        if(address >= range.begin && address < range.end) {
            return true;
        }
    }
    return false;
}

auto cs_assert() -> void {
    gpio::set(get_gpio_line(Pin::Cs), false);
}

auto cs_deassert() -> void {
    gpio::set(get_gpio_line(Pin::Cs), true);
}

// keep clocking until the card releases the bus (0xff)
auto wait_ready() -> bool {
    for(auto i = u32(0); i < max_bus_attempts; i += 1) {
        if(spi::xfer(0xff) == 0xff) {
            return true;
        }
    }
    return false;
}

// >74 clocks with cs and mosi high to put the card in spi mode
auto send_training_seq() -> void {
    const auto& mosi = get_gpio_line(Pin::Mosi);

    cs_deassert();
    gpio::set(mosi, true);
    gpio::configure(mosi, gpio::Mode::Output);
    for(auto i = 0; i < 16; i += 1) {
        spi::xfer(0xff);
    }
    gpio::set(mosi, false);
    gpio::configure(mosi, gpio::Mode::Alternate);
}

auto sdio_cmd(const u8 index, const u32 arg, u32* const rsp = nullptr) -> bool {
#pragma push_macro("error_act")
#define error_act      \
    {                  \
        cs_deassert(); \
        return false;  \
    }

    cs_assert();
    ensure(index == Cmd::MorseStartup || wait_ready());
    auto frame = noxx::Array<u8, 6>();
    frame[0]   = index | 0x40;
    frame[1]   = arg >> 24;
    frame[2]   = arg >> 16;
    frame[3]   = arg >> 8;
    frame[4]   = arg;
    frame[5]   = (index == Cmd::RWDirect || index == Cmd::RWExtended) ? u8(crc7(frame.data, 5) << 1 | 1) : u8(0xff);
    for(const auto byte : frame.data) {
        spi::xfer(byte);
    }
    // first non-0xff byte is the status, one more byte follows (r5)
    auto status = u8(0xff);
    auto data   = spi::xfer(0xff);
    for(auto i = u32(0); i < max_bus_attempts; i += 1) {
        status = data;
        data   = spi::xfer(0xff);
        if(status != 0xff) {
            break;
        }
    }
    ensure(status == 0x00);
    ensure(index != Cmd::RWExtended || data == 0x00);
    if(rsp != nullptr) {
        *rsp = data;
    }
    cs_deassert();
    return true;
#pragma pop_macro("error_act")
}

auto cmd52_write(const u32 address, const u8 data, const u32 func) -> bool {
    return sdio_cmd(Cmd::RWDirect, arg_write | func | address << arg_addr_bits | data);
}

// data phase after a CMD53 read command. block mode transfers `count` blocks of
// `block` bytes each (every block has its own start token and crc); byte mode
// (block == 0) transfers a single run of `count` bytes. mirrors morselib
// morse_cmd53_get_data.
auto cmd53_read_data(u8* data, const u32 count, const u32 block) -> bool {
#pragma push_macro("error_act")
#define error_act      \
    {                  \
        cs_deassert(); \
        return false;  \
    }

    const auto size      = block != 0 ? block : count;
    auto       remaining = block != 0 ? count : u32(1);

    cs_assert();
    while(remaining > 0) {
        remaining -= 1;
        auto token = u8(0xff);
        for(auto i = u32(0); i < max_bus_attempts; i += 1) {
            token = spi::xfer(0xff);
            if(token == tkn_start_block) {
                break;
            }
        }
        ensure(token == tkn_start_block);
        for(auto i = u32(0); i < size; i += 1) {
            data[i] = spi::xfer(0xff);
        }
        auto rx_crc = u16(spi::xfer(0xff)) << 8;
        rx_crc |= spi::xfer(0xff);
        ensure(crc16(data, size) == rx_crc);
        data += size;
    }
    cs_deassert();
    return true;
#pragma pop_macro("error_act")
}

// data phase after a CMD53 write command. block mode transfers `count` blocks of
// `block` bytes each (multi-block token stream); byte mode (block == 0) transfers
// a single run of `count` bytes. mirrors morselib morse_cmd53_put_data.
auto cmd53_write_data(const u8* data, const u32 count, const u32 block) -> bool {
#pragma push_macro("error_act")
#define error_act      \
    {                  \
        cs_deassert(); \
        return false;  \
    }

    const auto size      = block != 0 ? block : count;
    const auto start_tkn = (block != 0 && count > 1) ? tkn_multi_block : tkn_start_block;
    auto       remaining = block != 0 ? count : u32(1);

    cs_assert();
    while(remaining > 0) {
        remaining -= 1;
        const auto crc = crc16(data, size);
        ensure(wait_ready());
        spi::xfer(0xff);
        spi::xfer(start_tkn);
        for(auto i = u32(0); i < size; i += 1) {
            spi::xfer(data[i]);
        }
        spi::xfer(crc >> 8);
        spi::xfer(crc);
        auto token = u8(0xff);
        for(auto i = 0; i < 4; i += 1) {
            token = spi::xfer(0xff);
            if(token != 0xff) {
                break;
            }
        }
        ensure(token == tkn_data_accepted);
        data += size;
    }
    if(start_tkn == tkn_multi_block) {
        spi::xfer(tkn_stop_tran);
    }
    ensure(wait_ready());
    cs_deassert();
    return true;
#pragma pop_macro("error_act")
}

// point the 64KB access window at address & 0xffff0000
auto set_window(const u32 address, const u32 func) -> bool {
    constexpr auto error_value = false;

    ensure(cmd52_write(reg_window_low, address >> 16, func));
    ensure(cmd52_write(reg_window_high, address >> 24, func));
    ensure(cmd52_write(reg_window_access, access_4byte, func));
    return true;
}
} // namespace

auto read_u32(const u32 address) -> noxx::Optional<u32> {
    constexpr auto error_value = noxx::nullopt;

    const auto func = is_memory(address) ? arg_func2 : arg_func1;
    ensure(set_window(address, func));
    ensure(sdio_cmd(Cmd::RWExtended, func | arg_addr_inc | (address & 0xffff) << arg_addr_bits | 4));
    u8 data[4];
    ensure(cmd53_read_data(data, 4, 0));
    return u32(data[0]) | u32(data[1]) << 8 | u32(data[2]) << 16 | u32(data[3]) << 24;
}

// read `size` bytes (multiple of 4) from chip memory/registers, splitting across
// the 64KB access window and into block + byte CMD53 transfers. mirrors
// morselib morse_trns_read_multi_byte.
auto read_multi(u32 address, u8* data, u32 size) -> bool {
    constexpr auto error_value = false;

    ensure(size % 4 == 0);
    const auto func     = is_memory(address) ? arg_func2 : arg_func1;
    const auto block    = func == arg_func2 ? block_size_func2 : block_size_func1;
    const auto max_xfer = block * max_blocks;
    while(size > 0) {
        ensure(set_window(address, func));
        auto       chunk         = size < max_xfer ? size : max_xfer;
        const auto next_boundary = (address & 0xffff0000) + 0x10000;
        if(address + chunk > next_boundary) {
            chunk = next_boundary - address;
        }
        const auto blocks = chunk / block;
        if(blocks > 0) {
            const auto len = blocks * block;
            ensure(sdio_cmd(Cmd::RWExtended, func | arg_block_mode | arg_addr_inc |
                                                 (address & 0xffff) << arg_addr_bits | blocks));
            ensure(cmd53_read_data(data, blocks, block));
            address += len;
            data += len;
            size -= len;
            chunk -= len;
        }
        if(chunk > 0) {
            ensure(sdio_cmd(Cmd::RWExtended, func | arg_addr_inc |
                                                 (address & 0xffff) << arg_addr_bits | chunk));
            ensure(cmd53_read_data(data, chunk, 0));
            address += chunk;
            data += chunk;
            size -= chunk;
        }
    }
    return true;
}

auto write_u32(const u32 address, const u32 value) -> bool {
    const u8 data[4] = {u8(value), u8(value >> 8), u8(value >> 16), u8(value >> 24)};
    return write_multi(address, data, 4);
}

// write `size` bytes (multiple of 4) to chip memory/registers, splitting across
// the 64KB access window and into block + byte CMD53 transfers. mirrors
// morselib morse_trns_write_multi_byte.
auto write_multi(u32 address, const u8* data, u32 size) -> bool {
    constexpr auto error_value = false;

    const auto func     = is_memory(address) ? arg_func2 : arg_func1;
    const auto block    = func == arg_func2 ? block_size_func2 : block_size_func1;
    const auto max_xfer = block * max_blocks;
    while(size > 0) {
        ensure(set_window(address, func));
        auto       chunk         = size < max_xfer ? size : max_xfer;
        const auto next_boundary = (address & 0xffff0000) + 0x10000;
        if(address + chunk > next_boundary) {
            chunk = next_boundary - address;
        }
        const auto blocks = chunk / block;
        if(blocks > 0) {
            const auto len = blocks * block;
            ensure(sdio_cmd(Cmd::RWExtended, arg_write | func | arg_block_mode | arg_addr_inc |
                                                 (address & 0xffff) << arg_addr_bits | blocks));
            ensure(cmd53_write_data(data, blocks, block));
            address += len;
            data += len;
            size -= len;
            chunk -= len;
        }
        if(chunk > 0) {
            ensure(sdio_cmd(Cmd::RWExtended, arg_write | func | arg_addr_inc |
                                                 (address & 0xffff) << arg_addr_bits | chunk));
            ensure(cmd53_write_data(data, chunk, 0));
            address += chunk;
            data += chunk;
            size -= chunk;
        }
    }
    return true;
}

auto init() -> coop::Async<bool> {
    constexpr auto error_value = false;

    const auto& reset = get_gpio_line(Pin::Reset);
    const auto& wake  = get_gpio_line(Pin::Wake);
    const auto& cs    = get_gpio_line(Pin::Cs);
    const auto& busy  = get_gpio_line(Pin::Busy);
    const auto& irq   = get_gpio_line(Pin::Irq);
    const auto& sck   = get_gpio_line(Pin::Sck);
    const auto& miso  = get_gpio_line(Pin::Miso);
    const auto& mosi  = get_gpio_line(Pin::Mosi);

    // reset asserted (low), wake low, cs deasserted (high)
    gpio::set(reset, false);
    gpio::set(wake, false);
    gpio::set(cs, true);
    gpio::configure(reset, gpio::Mode::Output);
    gpio::configure(wake, gpio::Mode::Output);
    gpio::configure(cs, gpio::Mode::Output);

    // busy/irq inputs
    gpio::configure(busy, gpio::Pull::Down);
    gpio::configure(busy, gpio::Mode::Input);
    gpio::configure(irq, gpio::Mode::Input);

    // enable spi af
    gpio::configure(sck, gpio::Mode::Alternate);
    gpio::configure(miso, gpio::Mode::Alternate);
    gpio::configure(mosi, gpio::Mode::Alternate);

    // hard reset
    gpio::set(reset, false);
    co_await coop::sleep_ms(5);
    gpio::set(reset, true);
    co_await coop::sleep_ms(20);

    send_training_seq();
    for(auto i = 0; i < 3; i += 1) {
        if(sdio_cmd(Cmd::MorseStartup, 0)) {
            // the chip swallows the first command after startup, kick it with a dummy read
            sdio_cmd(Cmd::RWDirect, arg_func1 | reg_window_low << arg_addr_bits);
            co_return true;
        }
        sdio_cmd(Cmd::GoIdle, 0);
    }
    co_ensure(false); // chip not responding
}
} // namespace halow
