#pragma once
#include <coop/generator.hpp>
#include <noxx/string.hpp>

// console helpers
auto read_line() -> coop::Async<noxx::Optional<noxx::String>>;
