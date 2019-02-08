#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define FlagByte(Fields) \
union { uint8_t raw; struct Fields; }

constexpr auto bit(int n) -> uint8_t
{
	return 1 << n;
}
