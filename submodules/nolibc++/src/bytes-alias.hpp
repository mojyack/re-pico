#include "array.hpp"
#include "span.hpp"

#if !defined(NOXX_BYTES_ALIAS_H)

#define NOXX_BYTES_ALIAS_H
#pragma push_macro("bytes_alias")
#undef bytes_alias
#define bytes_alias(Name, size)                      \
    using Name         = noxx::Array<u8, size>;      \
    using Name##Ref    = noxx::Span<const u8, size>; \
    using Name##MutRef = noxx::Span<u8, size>
#else

#undef NOXX_BYTES_ALIAS_H
#pragma pop_macro("bytes_alias")

#endif
