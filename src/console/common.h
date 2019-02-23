#pragma once

#include <cstdint>

#define FLAG_BYTE(FIELDS) \
union { uint8_t raw; struct FIELDS; }

#define BETWEEN(VAL, LBOUND, RBOUND) \
	((VAL) >= (LBOUND) && (VAL) < (RBOUND))

template <typename T>
inline auto even(T x) -> bool { return x % 2 == 0; }

template <typename T>
inline auto odd(T x) -> bool { return !even(x); }

constexpr auto bit(int n) -> uint8_t
{
	return 1 << n;
}