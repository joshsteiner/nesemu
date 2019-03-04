#define BOOST_TEST_MODULE nestest
#include <boost/test/unit_test.hpp>

#include <sstream>
#include <vector>
#include <tuple>

#include "../src/console/cpu.h"
#include "../src/console/memory.h"
#include "../src/console/cart.h"
#include "../src/nesemu.h"
#include "../src/screen.h"

Cpu cpu;
Ppu ppu;
Memory memory;
Screen screen;


auto parse_next_instr(std::ifstream& file) -> CpuSnapshot
{
	unsigned pc;
	std::vector<uint8_t> instr;
	unsigned a, x, y, p, sp;
	unsigned cyc;

	file >> std::hex >> pc;

	for (;;) {
		std::string s;
		file >> s;
		if (s.length() != 2) { break; }
		instr.push_back(std::stoi(s, nullptr, 16));
	}

	file.ignore(100, ':'); file >> a;
	file.ignore(100, ':'); file >> x;
	file.ignore(100, ':'); file >> y;
	file.ignore(100, ':'); file >> p;
	file.ignore(100, ':'); file >> sp;
	file.ignore(100, ':'); file >> std::dec >> cyc;
	file.ignore(100, '\n');

	return CpuSnapshot{ pc, instr, a, x, y, p, sp, cyc };
}

auto run_testrom(std::string rom_filename, std::string log_filename) -> void
{
	Console console;
	console.load(rom_filename);

	std::ifstream log_file{ log_filename };
	if (!log_file.is_open()) { throw "log file failure"; }

	/*cpu.program_counter = 0xC000;
	cpu.status.raw = 0x24;
	cpu.stack_ptr = 0xFD;
	cpu.cycle = 0;*/

	cpu.a = 0xA0;
	cpu.status.raw = 0x85;
	cpu.stack_ptr = 0xFD;
	cpu.cycle = 30;
	ppu.cycle = 10;

	for (;;) {
		auto expected = parse_next_instr(log_file);
		auto got = cpu.take_snapshot();
		std::cout << str(expected) << '&';
		std::cout << str(got) << '\n';
		if (expected != got) {
			std::ostringstream fmt_err;
			fmt_err
				<< "expected: " << str(expected) << ';'
				<< "got: " << str(got);
			BOOST_FAIL(fmt_err.str());
		}
		console.step();
	}
}

BOOST_AUTO_TEST_CASE(nestest)
{
	//run_testrom("test/nestest.nes", "test/nestest.log");
}

BOOST_AUTO_TEST_CASE(color_test)
{
	run_testrom("test/color_test.nes", "test/color_test.log");
}
