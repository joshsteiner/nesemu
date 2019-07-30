import re
import sys

class Snapshot:
    def __init__(self, pc, instr, a, x, y, status, stack_ptr, cycle):
        self.pc = pc
        self.instr = instr
        self.a = a
        self.x = x
        self.y = y
        self.status = status
        self.stack_ptr = stack_ptr
        self.cycle = cycle

    def __eq__(self, other):
        return self.pc == other.pc \
            and self.instr == other.instr \
            and self.a == other.a \
            and self.x == other.x \
            and self.y == other.y \
            and self.status == other.status \
            and self.stack_ptr == other.stack_ptr \
            and self.cycle == other.cycle 

    def __str__(self):
        return "<PC={};instr={};A={};X={};Y={};status={};stack_ptr={};cycle={}>".format(
            self.pc, self.instr, self.a, self.x, self.y, self.status, 
            self.stack_ptr, self.cycle
            )
        

def parse(filename):
    def parse_line(line):
        return re.findall(r'(?:\=)([A-Za-z0-9])(?:\;|\})', line)

    with open(filename) as f:
        yield from map(parse_line, f)


if __name__ == '__main__':
    if len(sys.argv) == 3:
        rom = sys.argv[1]
        log = sys.argv[2]
    else:
        rom = "nestest.nes"
        log = "nestest.log"

    for exp, got in zip(parse(rom), parse(log)):
        print(exp, " | ", got)

