################################################################################
# CMakeLists.txt
#
# Root CMake build script for growt.
#
# Part of Project growt - https://github.com/TooBiased/growt.git
#
# Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
#
# All rights reserved. Published under the BSD-2 license in the LICENSE file.
################################################################################

cmake_minimum_required(VERSION 3.15)


project(growt
  VERSION 1.9
  LANGUAGES CXX)

set(GROWT_HASHFCT XXH3 CACHE STRING
  "Changes the used hash function if XXHASH is not available, MURMUR2 is used as backoff!")
set_property(CACHE GROWT_HASHFCT PROPERTY STRINGS XXH3 XXHASH MURMUR2 MURMUR3 CRC)

option(GROWT_BUILD_TESTS "Enables the tests." OFF)
option(GROWT_BUILD_EXAMPLE "Enables the examples." TRUE)
option(GROWT_GENERATE_COMPILE_COMMAND
       "Turns on construction compile_commands.json ." TRUE)

# find pthread
if(GROWT_BUILD_TESTS OR GROWT_BUILD_EXAMPLE)
  set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
  set(THREADS_PREFER_PTHREAD_FLAG TRUE)
  find_package(Threads REQUIRED)
endif()

##### SUBMODULE FINDS ##########################################################

if ((GROWT_HASHFCT STREQUAL XXHASH) OR
    (GROWT_HASHFCT STREQUAL XXH3))
  message(STATUS "xxHash is currently not supported")
  set(GROWT_HASHFCT MURMUR2)

  # message(STATUS "Looking xxhash")

  # find_path(XXHASH_DIR xxhash/xxhash.h
  #   PATHS ${CMAKE_CURRENT_SOURCE_DIR}/misc/submodules)
  #     #ENV ${CMAKE_CURRENT_SOURCE_DIR}/submodules) #ENV PATH ENV JUNCTION_ROOT)

  # if (XXHASH_DIR)
  #   include_directories(${XXHASH_DIR}/xxhash)
  #   message(STATUS "Looking for xxhash -- found")
  # else()
  #   message(STATUS "Looking for xxhash -- not found")
  #   message("Cannot find xxHash. Use MURMUR2 instead.")
  #   set(GROWT_HASHFCT MURMUR2)
  # endif()
endif()

if (GROWT_HASHFCT STREQUAL MURMUR3)
  message(STATUS "MURMUR3 currently not supported")
  set(GROWT_HASHFCT MURMUR2)
  # message(STATUS "Looking for smhasher")
  # find_path(MURMUR3_DIR MurmurHash3.cpp
  # PATHS ${CMAKE_CURRENT_SOURCE_DIR}/misc/submodules/smhasher)
  # if (MURMUR3_DIR)
  #   include_directories(${MURMUR3_DIR}/)
  #   message(STATUS "Looking for smhasher -- found")
  #   message(STATUS "${MURMUR3_DIR}")
  # else()
  #   message(STATUS "Looking for smhasher -- not found")
  #   message(STATUS "Maybe: the smhasher submodule is not initialized")
  # endif()
endif()

#add_executable(malloc malloc_count.c)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED On)
add_compile_options(-msse4.2 -mcx16 -Wall -Wextra)

include_directories("${PROJECT_SOURCE_DIR}")

if ( GROWT_BUILD_TESTS )
  message(STATUS "Building tests.")
  add_subdirectory(tests)
endif()

if ( GROWT_BUILD_EXAMPLE )
  message(STATUS "Building examples.")
  add_subdirectory(example)
endif()
