#pragma once
#include <noxx/optional.hpp>
#include <noxx/string-view.hpp>
#include <noxx/vector.hpp>

auto split(noxx::StringView str, noxx::StringView sep) -> noxx::Optional<noxx::Vector<noxx::StringView>>;
