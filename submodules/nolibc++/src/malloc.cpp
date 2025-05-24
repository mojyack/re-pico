#include "bits.hpp"

namespace noxx {
namespace {
struct ChunkHeader;

auto read_ptr(usize num) -> ChunkHeader* {
    return (ChunkHeader*)(num & ~usize(0b11));
}

auto set_ptr(usize& num, ChunkHeader* ptr) -> void {
    num = (usize)ptr | (num & 0b11);
}

/*
auto read_flag_1(usize num) -> bool {
    return num & 0b10;
}

auto set_flag_1(usize& num, bool flag) -> void {
    num = (num & ~usize(0b11)) | flag << 1 | (num & 0b01);
}
*/

auto read_flag_2(usize num) -> bool {
    return num & 0b01;
}

auto set_flag_2(usize& num, bool flag) -> void {
    num = (num & ~usize(0b01)) | flag;
}

struct ChunkHeader {
    // use last two bits of them as a flag
    usize next_r_free; // ..2: next, 1: reserved, 0: is_free
    usize prev_r_r;    // ..2: prev, 1..0: reserved

    auto next() -> ChunkHeader* {
        return read_ptr(next_r_free);
    }

    auto set_next(ChunkHeader* ptr) -> void {
        set_ptr(next_r_free, ptr);
    }

    auto is_free() -> bool {
        return read_flag_2(next_r_free);
    }

    auto set_is_free(bool flag) -> void {
        set_flag_2(next_r_free, flag);
    }

    auto prev() -> ChunkHeader* {
        return read_ptr(prev_r_r);
    }

    auto set_prev(ChunkHeader* ptr) -> void {
        set_ptr(prev_r_r, ptr);
    }

    auto size() -> usize {
        return ((u8*)next() - (u8*)this) - sizeof(ChunkHeader);
    }

    auto data() -> u8* {
        return (u8*)this + sizeof(ChunkHeader);
    }
};

constexpr auto chunk_header_align = lsb<alignof(ChunkHeader)>;

static_assert(chunk_header_align >= 2); // ensure rooms for flags

auto link(ChunkHeader& a, ChunkHeader& b) -> void {
    a.set_next(&b);
    b.set_prev(&a);
}

auto merge(ChunkHeader& a, ChunkHeader& b) -> void {
    const auto next = b.next();
    if(next != nullptr) {
        link(a, *next);
    } else {
        a.set_next(nullptr);
    }
}

auto head = (ChunkHeader*)(nullptr);
} // namespace

auto set_heap(void* const ptr, const usize size) -> void {
    head      = (ChunkHeader*)align_ceil(ptr, chunk_header_align);
    auto tail = (ChunkHeader*)align_floor((usize)ptr + size - sizeof(ChunkHeader), chunk_header_align);
    *head     = {0, 0};
    *tail     = {0, 0};
    link(*head, *tail);
    head->set_is_free(true);
}

auto malloc(const usize size) -> void* {
    const auto required_size = align_ceil(size + sizeof(ChunkHeader), chunk_header_align);
    for(auto chunk = head; chunk != nullptr; chunk = chunk->next()) {
        if(!chunk->is_free()) {
            continue;
        }
        if(chunk->size() < required_size) {
            continue;
        }
        // insert header
        auto new_chunk  = (ChunkHeader*)align_ceil(chunk->data() + size, chunk_header_align);
        *new_chunk      = {0, 0};
        const auto next = chunk->next();
        link(*new_chunk, *next);
        link(*chunk, *new_chunk);
        chunk->set_is_free(false);
        new_chunk->set_is_free(true);
        return chunk->data();
    }
    return nullptr;
}

auto free(void* const ptr) -> void {
    const auto chunk = (ChunkHeader*)((u8*)ptr - sizeof(ChunkHeader));
    const auto next  = chunk->next();
    const auto prev  = chunk->prev();
    chunk->set_is_free(true);
    if(next != nullptr && next->is_free()) {
        merge(*chunk, *next);
    }
    if(prev != nullptr && prev->is_free()) {
        merge(*prev, *chunk);
    }
}
} // namespace noxx

#if defined(NOXX_TEST)
#include <print>

namespace noxx {
auto dump_state() -> void {
    std::println("- chunk list");
    for(auto chunk = head; chunk != nullptr; chunk = chunk->next()) {
        std::println("chunk {}: next={} prev={} size={} free={}", (void*)chunk, (void*)chunk->next(), (void*)chunk->prev(), chunk->size(), chunk->is_free());
    }

    constexpr auto unit = alignof(ChunkHeader);

    std::print("- chunk graph(unit={}bytes)", unit);

    auto       pos = 0;
    const auto put = [&pos](const char c) {
        if(pos % 16 == 0) {
            std::print("\n{}  ", (void*)((u8*)head + pos * unit));
        }
        std::print("{}", c);
        pos += 1;
    };

    for(auto chunk = head; chunk != nullptr; chunk = chunk->next()) {
        for(auto i = 0uz; i < sizeof(ChunkHeader) / unit; i += 1) {
            put('H');
        }
        if(chunk->next() != nullptr) {
            const auto c = chunk->is_free() ? '.' : '*';
            for(auto i = 0uz; i < chunk->size() / unit; i += 1) {
                put(c);
            }
        }
    }
    std::println();
}
} // namespace noxx
#endif
