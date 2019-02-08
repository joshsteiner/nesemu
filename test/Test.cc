#define BOOST_TEST_MODULE nestest
#include <boost/test/unit_test.hpp>

#include <sstream>
#include <vector>
#include <tuple>

#include "../src/cpu.h"
#include "../src/cart.h"

#include "nestest.log.inl"

BOOST_AUTO_TEST_CASE(nestest)
{
	Cpu cpu;
	unsigned step_count = 0;
	
	std::ifstream file{ "nestest.nes", std::ios::binary };
	auto cart = Cartridge::from_ines(file);
	cpu.load_cart(cart);

	// JSR $C000
	// cpu.memory[0] = 0x20;
	// cpu.memory[1] = 0x00;
	// cpu.memory[2] = 0xC0;
	// cpu.step();

	// C000  4C F5 C5  JMP $C5F5                       A:00 X:00 Y:00 P:24 SP:FD CYC:  0

	cpu.program_counter = 0xC000;
	cpu.status.raw = 0x24;
	cpu.stack_ptr = 0xFD;
	cpu.cycle = 0;

	auto return_addr = cpu.program_counter + 3;
	
	try {
		CpuSnapshot snap;
		while ((snap = NestestLog[step_count]) != CpuSnapshot{}) {
			if (snap != cpu.take_snapshot()) {
				throw std::make_pair(snap, cpu.take_snapshot());
			}
			cpu.step();
			++step_count;
		}
	} catch (const char *msg) {
		std::ostringstream strm;
		strm 
			<< msg
			<< " PC=$" << std::hex << cpu.program_counter
			<< " step number:" << std::dec << step_count;
		BOOST_TEST_FAIL(strm.str());
	} catch (std::pair<CpuSnapshot, CpuSnapshot> diff) {
		auto [expected, got] = diff;
		std::ostringstream strm;
		strm
			<< "expected: " << expected.to_str() << '\n'
			<< "got: " << got.to_str();
		BOOST_TEST_FAIL(strm.str());
	}
}