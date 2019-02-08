cmake_minimum_required(VERSION 3.10)
project(nesemu CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(src)
include_directories(test)

link_directories("C:\\boost\\lib64-msvc-14.1")

add_executable(
	nesemu
	src/cart.cc
	src/cpu.cc
	# src/ppu.cc 
	src/main.cc
	)

add_executable(
	nestest
	test/Test.cc
	src/cpu.cc
	src/cart.cc
	)

target_include_directories(nestest PRIVATE "C:\\boost")

enable_testing()
add_test(NAME run_nestest COMMAND nestest)

