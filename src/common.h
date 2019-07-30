#ifndef NESEMU_COMMON_H
#define NESEMU_COMMON_H

//#include <ostream>
#include <iostream>
#include <cstdint>

#define FLAG_BYTE(FIELDS) \
	union { uint8_t raw; struct FIELDS; }

#define BETWEEN(VAL, LBOUND, RBOUND) \
	((VAL) >= (LBOUND) && (VAL) < (RBOUND))

#define BIT(N) (1 << (N))

template <typename T>
inline bool even(T x)
{
	return x % 2 == 0;
}

template <typename T>
inline bool odd(T x)
{
	return !even(x);
}

#if LOGGING_ENABLED
  #define LOG(MSG)          fprintf(stderr, "%s:%d:%s(): " MSG "\n", __FILE__, __LINE__, __func__)
  #define LOG_FMT(MSG, ...) fprintf(stderr, "%s:%d:%s(): " MSG "\n", __FILE__, __LINE__, __func__, __VA_ARGS__)
#else
  #define LOG(MSG)
  #define LOG_FMT(MSG, ...)
#endif

#endif
