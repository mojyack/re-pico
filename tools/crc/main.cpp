#include "CRC.h"
#include "macros/unwrap.hpp"
#include "util/file-io.hpp"

auto main(const int argc, const char* const* argv) -> int {
    ensure(argc == 3, "usage: crc INPUT_FILE OUTPUT_FILE");
    unwrap_mut(bin, read_file(argv[1]));
    bin.resize(std::max(256uz, bin.size()));
    auto& crc = *(std::bit_cast<uint32_t*>(bin.data() + 252));
    ensure(crc == std::bit_cast<uint32_t>(std::array{'c', 'r', 'c', 'p'}));
    crc = CRC::Calculate<uint32_t>(bin.data(), 252, CRC::CRC_32_MPEG2());
    ensure(write_file(argv[2], bin));
    return 0;
}
