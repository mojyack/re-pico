#pragma once
#include <noxx/span.hpp>

auto inflate(const noxx::Span<const u8> src, const noxx::Span<u8> dst) -> bool;
