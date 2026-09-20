# TablePlus port from new_int, 20 September 2026

Base: master/origin/master ab683ef (16 September 2026).
Reference: new_int 7987bc0860a52e3cecf2db098da03af6b8f2a866.

## Already upstream

- UInt128 columns, parser, ItemView and UUID alias separation: d4f8fa4.
  Do not reapply b207b0d or the obsolete UInt128/UUID portions of cc4cca5 and ff22f9b.
- Current master also contains generic LowCardinality support and new Bignum
  APIs. Preserve these implementations rather than copying old files wholesale.

## Selectively cherry-picked

- 2d914d1: Int256/UInt256 column, parser, item view and type support. Adapted
  to master's APIs and the final raw 32-byte representation from 1ea01b8.
  New type codes are appended to the enum, preserving master's existing codes.
- f386472: Decimal256 storage/parser support. Retained its parser regression
  test. Omitted its unrelated .DS_Store file. Added full-width string parsing,
  formatting and At256()/Append(Int256). Legacy At() detects values outside
  Int128 rather than silently truncating the high half as new_int did.
- 3d824a8: wrapped LowCardinality(Nullable(String)) decoding support. Kept
  master's newer generic nonwrapped LowCardinality implementation.

The ToString(Int128/UInt128/Int256/UInt256) API from 0e501a8, c7cbfb3,
cc4cca5, ff22f9b, 1ea01b8 and c91c3ad is retained in type_utils.h.
Its implementation now uses inline helpers (safe in multiple translation
units), upstream's selected Int128 backend, and checked full-width parsing.
It no longer requires direct Abseil use.

Do not cherry-pick merge commits 4fbe66c/6f1fbe0 or 7987bc0 (test removal).
Master already includes the merged upstream changes and the removed test is
valuable coverage for the retained feature.

## Adaptation and tests

- Generic LowCardinality(Int256/UInt256), including 32-byte defaults and
  dictionary insertion, matches master's new fixed-size dictionary support.
- Decimal precision must be 1..76 and scale must not exceed precision.
- The old Int128 return type is retained; use StringAt() or At256() for values
  outside that type's range. StringAt preserves the declared scale.
- LowCardinality(Decimal) remains outside upstream's supported dictionary
  types, as on new_int. This port does not claim to add it.
- wide256_ut.cpp and wide256_odr_ut.cpp cover extrema, malformed/overflow
  inputs, actual little-endian bytes, column wire roundtrips, full 76-digit
  Decimal256, legacy Int128 boundaries, dictionary roundtrips, and wrapped
  nullable-string wire decoding. The second translation unit checks linkage.
