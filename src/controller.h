#ifndef NESEMU_CONTROLLER_H
#define NESEMU_CONTROLLER_H

#include <array>
#include "common.h"

class Controllers {
public:
	uint8_t read_state(int controller_number);
	void write_strobe(bool value);
private:
	bool strobe;
	std::array<uint8_t, 2> joypads;
};

extern Controllers controllers;

#endif
