#include "controller.h"
#include "screen.h"

uint8_t Controllers::read_state(int controller_number)
{
	if (strobe) {
		return 0x40 | (screen.get_joypad_state(controller_number) & 1);
	}

	auto& state = joypads.at(controller_number);
	auto result = 0x40 | (state & 1);
	state = 0x80 | (state >> 1);
	return result;
}

void Controllers::write_strobe(bool value)
{
	if (strobe && !value) {
		joypads[0] = screen.get_joypad_state(0);
		joypads[1] = screen.get_joypad_state(1);
	}

	strobe = value;
}
