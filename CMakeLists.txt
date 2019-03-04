cmake_minimum_required(VERSION 3.10)
project(nesemu CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(src)
include_directories(test)

link_directories("C:\\boost\\lib64-msvc-14.1")
link_directories("C:\\SDL2\\lib\\x64")

add_executable(
	nesemu
	src/main.cpp
	src/screen.cpp
	src/console/memory.cpp
	src/console/cpu.cpp
	src/console/ppu.cpp
	)

add_executable(
	nestest
	test/nestest.cpp
	src/screen.cpp
	src/console/cpu.cpp
	src/console/ppu.cpp
	src/console/memory.cpp
	)

target_include_directories(nestest PRIVATE "C:\\boost")
target_include_directories(nestest PRIVATE "C:\\SDL2\\include")
target_link_libraries(nestest "C:\\SDL2\\lib\\x64\\SDL2.lib")

target_include_directories(nesemu PRIVATE "C:\\SDL2\\include")
target_link_libraries(nesemu "C:\\SDL2\\lib\\x64\\SDL2.lib")

enable_testing()
add_test(NAME run_nestest COMMAND nestest)

