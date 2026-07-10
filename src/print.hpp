#pragma once
#include <coop/generator.hpp>
#include <noxx/string-view.hpp>

auto print_blocking(noxx::StringView str) -> void;
auto println_blocking(noxx::StringView str) -> void;
auto print(noxx::StringView str) -> coop::Async<void>;
auto println(noxx::StringView str) -> coop::Async<void>;
