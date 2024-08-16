# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(LCC-Tortoise)

# Bring in the LCC library
#set(LIBLCC_ENABLE_STATIC_CONTEXT ON)
#add_subdirectory(TrainUtils/LCC)

# Zephyr does not want to play with third party libraries nicely:
# https://github.com/zephyrproject-rtos/zephyr/issues/8439
# So we do the highly annoying thing here of re-creating part of the liblcc cmake file
# in order to bring in all of the sources that we need
include(TrainUtils/LCC/lcc-version)

file(GLOB lcc-sources
    TrainUtils/LCC/*.c
    TrainUtils/LCC/*.h
)

target_sources(app PRIVATE 
	src/main.c 
	src/tortoise.c 
	src/lcc-tortoise-state.c
	src/dcc-decode.c
	src/power-handler.c
	src/firmware_upgrade.c
	${lcc-sources}
)
target_include_directories(app PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/TrainUtils/LCC
	${CMAKE_CURRENT_BINARY_DIR}/TrainUtils/LCC
)


string(REPLACE "." ";" VERSION_LIST ${LIBLCC_VERSION})
list(GET VERSION_LIST 0 LCC_VERSION_MAJOR)
list(GET VERSION_LIST 1 LCC_VERSION_MINOR)
list(GET VERSION_LIST 2 LCC_VERSION_PATCH)
target_compile_definitions(app PRIVATE
    LIBLCC_BUILD
    LIBLCC_MAJOR=${LCC_VERSION_MAJOR}
    LIBLCC_MINOR=${LCC_VERSION_MINOR}
    LIBLCC_MICRO=${LCC_VERSION_PATCH}
    LIBLCC_HAS_CONFIG_H
)
set(LIBLCC_DEBUG 0)
set(LIBLCC_ENABLE_STATIC_CONTEXT 1)
set(LIBLCC_EVENT_LIST_STATIC_SIZE 20)
configure_file( TrainUtils/LCC/lcc-config.h.cmake TrainUtils/LCC/lcc-config.h )
