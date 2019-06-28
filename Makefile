COMPILER=g++
FLAGS=-std=c++11 -g -Wall -pedantic -I/usr/include/SDL2
LINK_FLAGS=-lSDL2

SOURCE_DIR=src
INCLUDE_DIR=src
BUILD_DIR=build

nesemu: src/main.cpp build/cpu.o build/ppu.o build/memory.o build/screen.o build/cart.o
	$(COMPILER) $^ -o $@ $(FLAGS) $(LINK_FLAGS)

nestest: test/nestest.cpp build/cpu.o build/ppu.o build/memory.o build/screen.o build/cart.o
	$(COMPILER) $^ -o $@ $(FLAGS) $(LINK_FLAGS)

rominfo: tools/rominfo.cpp build/cart.o
	$(COMPILER) $^ -o $@ $(FLAGS)

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.cpp $(BUILD_DIR)
	$(COMPILER) -c $< -o $@ $(FLAGS)

$(BUILD_DIR):
	mkdir $(BUILD_DIR)
