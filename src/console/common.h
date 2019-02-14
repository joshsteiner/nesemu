#pragma once

#include <cstdint>

#define FlagByte(Fields) \
union { uint8_t raw; struct Fields; }

constexpr auto bit(int n) -> uint8_t
{
	return 1 << n;
}