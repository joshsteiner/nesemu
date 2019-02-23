#pragma once

#include <cstdint>
#include <string>

const size_t DisplayWidth = 256;
const size_t DisplayHeight = 240;

const std::string WindowTitle = "nesemu";

using Color = uint32_t;

class PixelBuffer
{
public:
	virtual ~PixelBuffer() {}
public:
	virtual auto get(unsigned r, unsigned c) -> Color = 0;
	virtual auto set(unsigned r, unsigned c, Color color) -> void = 0;
	virtual auto operator<<(Color pixel) -> PixelBuffer& = 0;
	virtual auto update() -> void = 0;
};
