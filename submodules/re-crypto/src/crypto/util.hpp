#pragma once
#define bytes_alias(Name, size)                      \
    using Name         = noxx::Array<u8, size>;      \
    using Name##Ref    = noxx::Span<const u8, size>; \
    using Name##MutRef = noxx::Span<u8, size>
