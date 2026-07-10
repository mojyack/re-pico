#pragma once
#include "math.hpp"
#include "optional.hpp"
#include "type-traits.hpp"
#include "utility.hpp"

namespace noxx {
template <class... Ts>
class Variant {
  private:
    template <usize n>
    using E = NthType<n, Ts...>;

    template <class T>
    static constexpr auto false_v = false;

    static constexpr auto invalid_index = usize(-1);

    alignas(Ts...) char data[max(sizeof(Ts)...)];
    usize index = invalid_index;

    template <usize n, class T>
    static consteval auto find_index_of() -> usize {
        if constexpr(n < sizeof...(Ts)) {
            if constexpr(is_same<E<n>, T>) {
                return n;
            } else {
                return find_index_of<n + 1, T>();
            }
        } else {
            static_assert(false_v<T>, "no such type");
        }
    }

    template <usize n>
    static auto apply_void(auto self, auto visitor) -> bool {
        if constexpr(n < sizeof...(Ts)) {
            if(n == self->index) {
                visitor(self->template as<E<n>>());
                return true;
            }
            return apply_void<n + 1>(self, visitor);
        }

        return false;
    }

    template <usize n, class R>
    static auto apply_result(auto self, auto visitor) -> Optional<R> {
        if constexpr(n < sizeof...(Ts)) {
            if(n == self->index) {
                return visitor(self->template as<E<n>>());
            }
            return apply_result<n + 1, R>(self, visitor);
        }

        return nullopt;
    }

    static auto apply(auto self, auto visitor) -> decltype(auto) {
        using R = decltype(visitor(self->template as<E<0>>()));
        if constexpr(is_same<R, void>) {
            if(!self->is_valid()) {
                return false;
            }
            return apply_void<0>(self, visitor);
        } else {
            if(!self->is_valid()) {
                return Optional<R>(nullopt);
            }
            return apply_result<0, R>(self, visitor);
        }
    }

    template <bool is_move>
    static auto assign(Variant* const self, auto other) -> void {
        if(!other->is_valid()) {
            self->reset();
            return;
        }

        if(self->index == other->index) {
            self->apply([other](auto& v) {
                other->apply([other, &v](auto& u) {
                    using V = RemoveCvRef<decltype(v)>;
                    using U = RemoveCvRef<decltype(u)>;
                    if constexpr(is_same<V, U>) {
                        if constexpr(is_move) {
                            v = move(u);
                            other->reset();
                        } else {
                            v = u;
                            (void)other;
                        }
                    } else {
                        // should not reach here
                    }
                });
            });
            return;
        }

        self->reset();
        other->apply([self, other](auto& v) {
            using T = RemoveCvRef<decltype(v)>;
            if constexpr(is_move) {
                self->emplace<T>(move(v));
                other->reset();
            } else {
                self->emplace<T>(v);
                (void)other;
            }
        });
        return;
    }

  public:
    template <class T>
    static constexpr auto index_of = find_index_of<0, T>();

    auto get_index() const -> usize {
        return index;
    }

    template <class T>
    auto get() -> T* {
        return const_cast<T*>(((const Variant*)this)->template get<T>());
    }

    template <class T>
    auto get() const -> const T* {
        if(index_of<T> == index) {
            return (const T*)data;
        } else {
            return nullptr;
        }
    }

    template <class T>
    auto as() -> T& {
        return as<index_of<T>>();
    }

    template <class T>
    auto as() const -> const T& {
        return as<index_of<T>>();
    }

    template <usize n>
    auto as() -> E<n>& {
        return *(E<n>*)data;
    }

    template <usize n>
    auto as() const -> const E<n>& {
        return *(const E<n>*)data;
    }

    auto apply(auto visitor) -> decltype(auto) {
        return apply(this, visitor);
    }

    auto apply(auto visitor) const -> decltype(auto) {
        return apply(this, visitor);
    }

    auto is_valid() const -> bool {
        return index != invalid_index;
    }

    template <class T, class... Args>
    auto emplace(Args&&... args) -> T& {
        reset();
        if constexpr(is_constructible<T, Args...>) {
            new((T*)data) T(forward<Args>(args)...);
        } else {
            new((T*)data) T{{forward<Args>(args)}...};
        }
        index = index_of<T>;
        return as<T>();
    }

    auto reset() -> void {
        if(!is_valid()) {
            return;
        }
        apply([](auto& v) { destroy_at(&v); });
        index = invalid_index;
    }

    template <class T, class... Args>
    static auto create(Args&&... args) -> Variant {
        auto ret = Variant();
        ret.template emplace<T>(forward<Args>(args)...);
        return ret;
    }

    auto operator=(Variant& o) -> Variant& {
        assign<false>(this, &o);
        return *this;
    }

    auto operator=(const Variant& o) -> Variant& {
        assign<false>(this, &o);
        return *this;
    }

    auto operator=(Variant&& o) -> Variant& {
        assign<true>(this, &o);
        return *this;
    }

    Variant() = default;

    Variant(Variant& o) {
        assign<false>(this, &o);
    }

    Variant(const Variant& o) {
        assign<false>(this, &o);
    }

    Variant(Variant&& o) noexcept {
        assign<true>(this, &o);
    }

    ~Variant() {
        reset();
    }
};
} // namespace noxx
