typedef uint8_t *(*mode_impl)(void);

struct mode {
	mode_impl impl;
	const char *fmt;
	const char *name;
	enum { NONE=0, BYTE=1, ADDR=2 } arg;
};


#define MODE( NAME, ARG, FMT, GET )                  \
static uint8_t *NAME##_impl(void) { return GET; }    \
static const struct mode NAME = {                    \
	.impl = NAME##_impl,                         \
	.arg  = ARG,                                 \
	.fmt  = FMT,                                 \
	.name = #NAME                                \
};


MODE( IMPLIED,      NONE,  "%s         ",    NULL )
MODE( ACCUMULATOR,  NONE,  "%s A       ",    &CPU.A )
MODE( IMMEDIATE,    BYTE,  "%s #$%02X    ",  &CPU.mem[CPU.PC + 1] )
MODE( RELATIVE,     BYTE,  "%s $%04X   ",    &CPU.mem[CPU.PC + (int8_t)peek_arg() + 2] )
MODE( ZERO_PAGE,    BYTE,  "%s $%02X     ",  &CPU.mem[peek_arg()] )
MODE( ZERO_PAGE_X,  BYTE,  "%s $%02X,X   ",  &CPU.mem[(peek_arg() + CPU.X) % PAGE_SIZE] )
MODE( ZERO_PAGE_Y,  BYTE,  "%s $%02X,Y   ",  &CPU.mem[(peek_arg() + CPU.Y) % PAGE_SIZE] )
MODE( ABSOLUTE,     ADDR,  "%s $%04X   ",    &CPU.mem[peek_addr_arg()] )
MODE( ABSOLUTE_X,   ADDR,  "%s $%04X,X ",    &CPU.mem[(peek_addr_arg() + CPU.X) % MEMORY_SIZE] )
MODE( ABSOLUTE_Y,   ADDR,  "%s $%04X,Y ",    &CPU.mem[(peek_addr_arg() + CPU.Y) % MEMORY_SIZE] )
MODE( INDIRECT,     ADDR,  "%s ($%04X) ",    &CPU.mem[read_addr(peek_addr_arg())] )
MODE( INDIRECT_X,   BYTE,  "%s ($%02X,X) ",  &CPU.mem[read_addr((peek_arg() + CPU.X) % PAGE_SIZE)] ) 
MODE( INDIRECT_Y,   BYTE,  "%s ($%02X),Y ",  &CPU.mem[(read_addr(peek_arg() + CPU.Y)) % PAGE_SIZE] )