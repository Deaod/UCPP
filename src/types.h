#pragma once

#include <cstddef>
#include <cstdint>

using i8 = std::int8_t;
using u8 = std::uint8_t;
using i16 = std::int16_t;
using u16 = std::uint16_t;
using i32 = std::int32_t;
using u32 = std::uint32_t;
using i64 = std::int64_t;
using u64 = std::uint64_t;

#define STRINGIFY(str) STRINGIFY_INNER(str)
#define STRINGIFY_INNER(str) #str

#ifdef DEBUG
#define MSG_DEBUG "(" __FILE__ ":" STRINGIFY(__LINE__) ") "
#else
#define MSG_DEBUG
#endif
