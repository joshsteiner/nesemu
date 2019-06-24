#include <sstream>
#include <vector>
#include <tuple>

#include "../src/cpu.h"
#include "../src/memory.h"
#include "../src/cart.h"
#include "../src/nesemu.h"
#include "../src/screen.h"

Cpu cpu;
Ppu ppu;
Memory memory;
Screen screen;
std::ostream& logger = std::clog;

Cpu_snapshot parse_next_instr(std::ifstream& file)
{
	unsigned pc;
	std::vector<uint8_t> instr;
	unsigned a, x, y, p, sp;
	unsigned cyc;

	file >> std::hex >> pc;

	for (;;) {
		std::string s;
		file >> s;
		if (s.length() != 2) { 
			break; 
		}
		instr.push_back(std::stoi(s, nullptr, 16));
	}

	file.ignore(100, ':'); file >> a;
	file.ignore(100, ':'); file >> x;
	file.ignore(100, ':'); file >> y;
	file.ignore(100, ':'); file >> p;
	file.ignore(100, ':'); file >> sp;
	file.ignore(100, ':'); file >> std::dec >> cyc;
	file.ignore(100, '\n');

	return Cpu_snapshot{ pc, instr, a, x, y, p, sp, cyc };
}

void run_testrom(const std::string& rom_filename, const std::string& log_filename)
{
	Console console;
	console.load(rom_filename);

	std::ifstream log_file{ log_filename };
	if (!log_file.is_open()) { 
		throw "log file failure"; 
	}

	cpu.program_counter = 0xC000;
	cpu.status.raw = 0x24;
	cpu.stack_ptr = 0xFD;
	cpu.cycle = 0;

	/*cpu.a = 0xA0;
	cpu.status.raw = 0x85;
	cpu.stack_ptr = 0xFD;
	cpu.cycle = 30;
	ppu.cycle = 10;*/

	for (;;) {
		auto expected = parse_next_instr(log_file);
		auto got = cpu.take_snapshot();
		logger << expected.str() << "  &  " << got.str() << '\n';
		if (expected != got) {
			std::ostringstream fmt_err;
			std::cerr
				<< std::endl
				<< "expected: " << expected.str() << std::endl
				<< "got: " << got.str()
				<< std::endl;
			exit(-2);
		}
		console.step();
	}
}

int main(int argc, char** argv)
{
	if (argc != 3) {
		std::cerr << "USAGE: nestest nestest.nes nestest.log" << std::endl;
		std::exit(-1);
	}

	try {
		run_testrom(std::string{ argv[1] }, std::string{ argv[2] });
	} catch (std::string& s) {
		std::cerr << s << std::endl;
	} catch (const char* s) {
		std::cerr << s << std::endl;
	}

	return 0;
}

