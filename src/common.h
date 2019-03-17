#ifndef NESEMU_COMMON_H
#define NESEMU_COMMON_H

#include <cstdint>

#define FLAG_BYTE(FIELDS) \
union { uint8_t raw; struct FIELDS; }

#define BETWEEN(VAL, LBOUND, RBOUND) \
	((VAL) >= (LBOUND) && (VAL) < (RBOUND))

template <typename T>
inline bool even(T x) { return x % 2 == 0; }

template <typename T>
inline bool odd(T x) { return !even(x); }

constexpr uint8_t bit(int n)
{
	return 1 << n;
}

#endif
