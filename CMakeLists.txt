# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(LCC-Tortoise)

# Bring in the LCC library
include(FetchContent)

FetchContent_Declare(
	lcc
	SOURCE_DIR /home/robert/TrainUtils/LCC
	CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/lcc-install
	OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(lcc)
find_package(liblcc REQUIRED)

target_sources(app PRIVATE 
	src/main.c 
	src/tortoise.c 
	src/lcc-tortoise-state.c
)

target_link_libraries(app PRIVATE lcc)
add_definitions(-DARDUINO)
