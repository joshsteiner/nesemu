#ifndef NESEMU_MAPPERS_MAPPER0_H
#define NESEMU_MAPPERS_MAPPER0_H

#include "../cart.h"

class Mapper0 : public Mapper {
public:
	virtual uint8_t read(Extended_addr addr) override;
	virtual void write(Extended_addr addr, uint8_t value) override;

private:
	size_t banks_count;
	int banks[2];
};

#endif