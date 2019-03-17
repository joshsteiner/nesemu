COMPILER=g++
FLAGS=-std=c++11 -g -Wall -pedantic -I/usr/include/SDL2
LINK_FLAGS=-lSDL2

SOURCE_DIR=src
INCLUDE_DIR=src
BUILD_DIR=build

nesemu: src/main.cpp build/cpu.o build/ppu.o build/memory.o build/screen.o
	$(COMPILER) $^ -o nesemu $(FLAGS) $(LINK_FLAGS)

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.cpp
	$(COMPILER) -c $^ -o $@ $(FLAGS)
