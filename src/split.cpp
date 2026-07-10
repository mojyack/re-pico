#include <noxx/utility.hpp>

#include "split.hpp"

#include <noxx/assert.hpp>

auto split(const noxx::StringView str, const noxx::StringView sep) -> noxx::Optional<noxx::Vector<noxx::StringView>> {
    constexpr auto error_value = noxx::nullopt;

    auto ret = noxx::Vector<noxx::StringView>();
    auto pos = 0;
    while(true) {
        if(pos >= str.size()) {
            break;
        }
        const auto prev = noxx::exchange(pos, str.find(sep, pos));
        if(pos < 0) {
            if(prev != str.size()) {
                ensure(ret.append(str.substr(prev)));
            }
            break;
        }

        if(pos - prev > 0) {
            ensure(ret.append(str.substr(prev, pos - prev)));
        }

        pos += sep.size();
    }
    return ret;
}
