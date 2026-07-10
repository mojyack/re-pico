#include <print>

#include "noxx/variant.hpp"

#include "noxx/assert.hpp"

namespace {
constexpr auto error_value = false;

auto trivial() -> bool {
    using V = noxx::Variant<int, float>;

    ensure(!V().is_valid());

    auto v = V::create<int>(1);
    auto u = V::create<float>(1.0);
    ensure(*v.get<int>() == 1);
    ensure(*u.get<float>() == 1.0);

    auto e = V();
    v      = e;
    ensure(!v.is_valid());

    v = noxx::move(u);
    ensure(*v.get<float>() == 1.0);
    ensure(!u.is_valid());
    return true;
}

struct Count {
    int constructor      = 0;
    int copy_constructor = 0;
    int move_constructor = 0;
    int copy_assign      = 0;
    int move_assign      = 0;
    int destructor       = 0;
};

struct Noisy {
    Count* count;

    auto operator=(Noisy&&) {
        count->move_assign += 1;
    }

    auto operator=(Noisy&) {
        count->copy_assign += 1;
    }

    Noisy(Count* const count) : count(count) {
        count->constructor += 1;
    }

    Noisy(Noisy&& o) {
        count = o.count;
        count->move_constructor += 1;
    }

    Noisy(Noisy& o) {
        count = o.count;
        count->copy_constructor += 1;
    }

    ~Noisy() {
        count->destructor += 1;
    }
};

auto nontrivial() -> bool {
    using V = noxx::Variant<Noisy>;

    // constructor/destructor test
    auto v  = V();
    auto cv = Count();

    v.emplace<Noisy>(&cv);
    ensure(cv.constructor == 1);

    v.reset();
    ensure(cv.destructor == 1);

    // copy/move constructor test of contents
    cv      = Count();
    auto u  = V();
    auto cu = Count();

    v.emplace<Noisy>(&cu);

    u = v;
    ensure(cu.copy_constructor == 1 && v.is_valid());

    u.reset();
    u = noxx::move(v);
    ensure(cu.move_constructor == 1 && !v.is_valid());

    // copy/move assignment test
    cv = Count();
    cu = Count();
    v.emplace<Noisy>(&cv);
    u.emplace<Noisy>(&cu);

    u = v;
    ensure(cu.copy_assign == 1 && v.is_valid());

    u = noxx::move(v);
    ensure(cu.move_assign == 1 && !v.is_valid());

    // constructor test of variant
    cv = Count();
    cu = Count();

    auto x = u;
    ensure(cu.copy_constructor == 1 && u.is_valid());
    auto y = noxx::move(u);
    ensure(cu.move_constructor == 1 && !u.is_valid());
    return true;
}
} // namespace

auto main() -> int {
    constexpr auto error_value = -1;

    ensure(trivial());
    ensure(nontrivial());
    std::println("pass");
    return 0;
}
