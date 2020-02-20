#ifndef NESEMU_COMMON_H
#define NESEMU_COMMON_H

#include <iostream>
#include <cstdint>

#define FLAG_BYTE(FIELDS) \
	union { uint8_t raw; struct FIELDS; }

#define BETWEEN(VAL, LBOUND, RBOUND) \
	((VAL) >= (LBOUND) && (VAL) < (RBOUND))

#define BIT(N) (1 << (N))

inline void global_error [[noreturn]] (const char* msg, const char* file, unsigned line, const char* func)
{
  fprintf(stderr, "error in [%s:%d:%d] -- %d\n", msg);
  exit(EXIT_FAILURE);
}

#define GLOBAL_ERROR(MSG) \
  global_error((MSG), __FILE__, __LINE__, __func__)

// negative numbers point to registers
// non-negative to normal memory addresses
using Extended_addr = int;
const Extended_addr a_register_ext_addr = -1;

#if LOGGING_ENABLED
  #define LOG(MSG)          fprintf(stderr, "%s:%d:%s(): " MSG "\n", __FILE__, __LINE__, __func__)
  #define LOG_FMT(MSG, ...) fprintf(stderr, "%s:%d:%s(): " MSG "\n", __FILE__, __LINE__, __func__, __VA_ARGS__)
#else
  #define LOG(MSG)
  #define LOG_FMT(MSG, ...)
#endif

#endif
