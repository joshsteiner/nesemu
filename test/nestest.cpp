#define BOOST_TEST_MODULE nestest
#include <boost/test/unit_test.hpp>

#include <sstream>
#include <vector>
#include <tuple>

#include "../src/console/cpu.h"
#include "../src/console/cart.h"

auto parse_next_instr(std::ifstream& file) -> CpuSnapshot
{
	CpuSnapshot snapshot;
	file >> std::hex >> (int&)snapshot.program_counter;

	std::vector<uint8_t> instr;
	for (;;) {
		std::string s;
		file >> s;
		if (s.length() != 2) { break; }
		instr.push_back(std::stoi(s, nullptr, 16));
	}
	snapshot.curr_instr = std::move(instr);

	int i;

	file.ignore(100, ':');
	file >> i;
	snapshot.a = i;

	file.ignore(100, ':');
	file >> i;
	snapshot.x = i;

	file.ignore(100, ':');
	file >> i;
	snapshot.y = i;
	
	file.ignore(100, ':');
	file >> i;
	snapshot.status.raw = i;
	
	file.ignore(100, ':');
	file >> i; 
	snapshot.stack_ptr = i;
	
	file.ignore(100, ':');
	file >> std::dec >> snapshot.cycle;

	return snapshot;
}

BOOST_AUTO_TEST_CASE(nestest)
{
	Cpu cpu;
	unsigned step_count = 0;
	
	std::ifstream nestest_file{ "nestest.nes", std::ios::binary };
	auto cart = Cartridge::from_ines(nestest_file);
	nestest_file.close();
	cpu.load_cart(cart);

	std::ifstream log_file{ "nestest.log" };
	if (!log_file.is_open()) {
		throw "log file fail";
	}

	cpu.program_counter = 0xC000;
	cpu.status.raw = 0x24;
	cpu.stack_ptr = 0xFD;
	cpu.cycle = 0;

	auto return_addr = cpu.program_counter + 3;
	
	try {
		for (;;) {
			auto snap = parse_next_instr(log_file);
			if (snap.program_counter == 0xC6BD) {
				// test succeded
				break;
			}
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