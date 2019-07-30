COMPILER = g++

FLAGS_COMMON  = -std=c++11 -I/usr/include/SDL2 
FLAGS_DEBUG   = $(FLAGS_COMMON) -g -Wall -pedantic -DLOGGING_ENABLED
FLAGS_RELEASE = $(FLAGS_COMMON) -O2 

LINK_FLAGS = -lSDL2

OBJS           = cpu ppu memory screen cart
OBJS_CPP       = $(patsubst %, src/%.cpp, $(OBJS))
OBJS_H         = $(patsubst %, src/%.h, $(OBJS))
OBJS_RELEASE_O = $(patsubst %, build/release/%.o, $(OBJS))
OBJS_DEBUG_O   = $(patsubst %, build/debug/%.o, $(OBJS))

nesemu: src/main.cpp $(OBJS_RELEASE_O)
	$(COMPILER) $^ -o $@ $(FLAGS_RELEASE) $(LINK_FLAGS)

debug: src/main.cpp $(OBJS_DEBUG_O)
	$(COMPILER) $^ -o nesemu_dbg $(FLAGS_DEBUG) $(LINK_FLAGS)

nestest: test/nestest.cpp $(OBJS_DEBUG_O)
	$(COMPILER) $^ -o $@ $(FLAGS_DEBUG) $(LINK_FLAGS)

rominfo: tools/rominfo.cpp build/debug/cart.o
	$(COMPILER) $^ -o $@ $(FLAGS)

build/release/%.o: src/%.cpp src/%.h build/release
	$(COMPILER) -c $< -o $@ $(FLAGS_RELEASE)

build/debug/%.o: src/%.cpp src/%.h build/debug
	$(COMPILER) -c $< -o $@ $(FLAGS_DEBUG)

build/debug:
	mkdir -p build/debug

build/release:
	mkdir -p build/release

