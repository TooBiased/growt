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

cmake_minimum_required (VERSION 3.0)

# custom cmake scripts
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/misc/cmake)

project (concurrent-growtable)


# prohibit in-source builds
if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
  message(SEND_ERROR "In-source builds are not allowed.")
endif()

set(GROWT_BUILD_MODE RELEASE CACHE STRING
  "the build mode of the current tests (controls optimizations and symbols)!")
set_property(CACHE GROWT_BUILD_MODE PROPERTY STRINGS RELEASE DEBUG REL_W_SYMBOLS )

set(GROWT_ALLOCATOR ALIGNED CACHE STRING
  "Specifies the used allocator (only relevant for our tables)!")
set_property(CACHE GROWT_ALLOCATOR PROPERTY STRINGS ALIGNED)

set(GROWT_HASHFCT XXH3 CACHE STRING
  "Changes the used hash function if XXHASH is not available, MURMUR2 is used as backoff!")
set_property(CACHE GROWT_HASHFCT PROPERTY STRINGS XXH3 XXHASH MURMUR2 MURMUR3 CRC)

set (OLD_FLAGS ${CMAKE_CXX_FLAGS})

set (FLAGS "-std=c++20 -msse4.2 -mcx16 -Wall -Wextra")

if (GROWT_BUILD_MODE STREQUAL DEBUG)
set (FLAGS "${FLAGS} -g3 -ggdb -O0")
endif()
if (GROWT_BUILD_MODE STREQUAL RELEASE)
set (FLAGS "${FLAGS} -march=native -O3")
endif()
if (GROWT_BUILD_MODE STREQUAL REL_W_SYMBOLS)
set (FLAGS "${FLAGS} -g3 -ggdb -march=native -O3")
endif()

include_directories(.)

# find pthread
find_package(Threads REQUIRED)
set(TEST_DEP_LIBRARIES ${CMAKE_THREAD_LIBS_INIT} ${TEST_DEP_LIBRARIES})
#
# COMPILE THE EXAMPLE CODE (example/example.cpp)
add_executable(example example/example.cpp)
set_target_properties(example PROPERTIES COMPILE_FLAGS "${FLAGS}")
target_link_libraries(example ${TEST_DEP_LIBRARIES})

add_executable(range_example example/range_example.cpp)
set_target_properties(range_example PROPERTIES COMPILE_FLAGS "${FLAGS}")
target_link_libraries(range_example ${TEST_DEP_LIBRARIES})

##### SUBMODULE FINDS ##########################################################

if ((GROWT_HASHFCT STREQUAL XXHASH) OR
    (GROWT_HASHFCT STREQUAL XXH3))
  message(STATUS "Looking for xxhash")

  find_path(XXHASH_DIR xxhash/xxhash.h
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/misc/submodules)
      #ENV ${CMAKE_CURRENT_SOURCE_DIR}/submodules) #ENV PATH ENV JUNCTION_ROOT)

  if (XXHASH_DIR)
    include_directories(${XXHASH_DIR}/xxhash)
    message(STATUS "Looking for xxhash -- found")
  else()
    message(STATUS "Looking for xxhash -- not found")
    message("Cannot find xxHash. Use MURMUR2 instead.")
    set(GROWT_HASHFCT MURMUR2)
  endif()
endif()

if (GROWT_HASHFCT STREQUAL MURMUR3)
  message(STATUS "Looking for smhasher")
  find_path(MURMUR3_DIR MurmurHash3.cpp
  PATHS ${CMAKE_CURRENT_SOURCE_DIR}/misc/submodules/smhasher)
  if (MURMUR3_DIR)
    include_directories(${MURMUR3_DIR}/)
    message(STATUS "Looking for smhasher -- found")
    message(STATUS "${MURMUR3_DIR}")
  else()
    message(STATUS "Looking for smhasher -- not found")
    message(STATUS "Maybe: the smhasher submodule is not initialized")
  endif()
endif()

#add_executable(malloc malloc_count.c)

function( GrowTExecutable variant cpp directory name )
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${directory})
  add_executable(${name} tests/${cpp}.cpp)
  set_target_properties(${name} PROPERTIES COMPILE_FLAGS "${FLAGS}")
  target_compile_definitions(${name} PRIVATE
    -D ${variant}
    -D ${GROWT_HASHFCT}
    -D ${GROWT_ALLOCATOR}
    -D GROWT_USE_CONFIG)
  target_link_libraries(${name} PRIVATE ${TEST_DEP_LIBRARIES} ${ALLOC_LIB})
endfunction( GrowTExecutable )

function( GrowXExecutable variant cpp directory name )
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${directory})
  add_executable(${name} tests/${cpp}.cpp)
  set_target_properties(${name} PROPERTIES COMPILE_FLAGS "${FLAGS} -mrtm")
  target_compile_definitions(${name} PRIVATE
    -D ${variant}
    -D ${GROWT_HASHFCT}
    -D ${GROWT_ALLOCATOR}
    -D GROWT_USE_CONFIG)
  target_link_libraries(${name} PRIVATE ${TEST_DEP_LIBRARIES} ${ALLOC_LIB})
endfunction( GrowXExecutable )

GrowTExecutable( UAGROW functionality fun functionality_uaGrowT )
GrowTExecutable( USGROW functionality fun functionality_usGrowT )
GrowTExecutable( PAGROW functionality fun functionality_paGrowT )
GrowTExecutable( PSGROW functionality fun functionality_psGrowT )

GrowTExecutable( FOLKLORE ins_test ins ins_none_folklore )
GrowTExecutable( FOLKLORE mix_test mix mix_none_folklore )
GrowTExecutable( FOLKLORE con_test con con_none_folklore )
GrowTExecutable( FOLKLORE agg_test agg agg_none_folklore )

GrowTExecutable( SEQUENTIAL ins_test seq ins_full_sequential )
GrowTExecutable( SEQUENTIAL mix_test seq mix_full_sequential )
GrowTExecutable( SEQUENTIAL con_test seq con_full_sequential )
GrowTExecutable( SEQUENTIAL agg_test seq agg_full_sequential )

GrowTExecutable( UAGROW ins_test ins ins_full_uaGrowT )
GrowTExecutable( USGROW ins_test ins ins_full_usGrowT )
GrowTExecutable( PAGROW ins_test ins ins_full_paGrowT )
GrowTExecutable( PSGROW ins_test ins ins_full_psGrowT )
GrowTExecutable( UAGROW mix_test mix mix_full_uaGrowT )
GrowTExecutable( USGROW mix_test mix mix_full_usGrowT )
GrowTExecutable( PAGROW mix_test mix mix_full_paGrowT )
GrowTExecutable( PSGROW mix_test mix mix_full_psGrowT )
GrowTExecutable( UAGROW del_test del del_full_uaGrowT )
GrowTExecutable( USGROW del_test del del_full_usGrowT )
GrowTExecutable( PAGROW del_test del del_full_paGrowT )
GrowTExecutable( PSGROW del_test del del_full_psGrowT )
GrowTExecutable( UAGROW con_test con con_full_uaGrowT )
GrowTExecutable( USGROW con_test con con_full_usGrowT )
GrowTExecutable( PAGROW con_test con con_full_paGrowT )
GrowTExecutable( PSGROW con_test con con_full_psGrowT )
GrowTExecutable( UAGROW agg_test agg agg_full_uaGrowT )
GrowTExecutable( USGROW agg_test agg agg_full_usGrowT )
GrowTExecutable( PAGROW agg_test agg agg_full_paGrowT )
GrowTExecutable( PSGROW agg_test agg agg_full_psGrowT )
# target_compile_definitions(del_full_uaGrowT PRIVATE
#   -D CMAP)

GrowTExecutable( FOLKLORE ins32_test  ins32  ins32_none_folklore )
GrowTExecutable( UAGROW   ins32_test  ins32  ins32_full_uaGrowT )


if (GROWT_BUILD_MEMORY_TEST)
  GrowTExecutable( FOLKLORE ins_test mem mem_none_folklore )
  target_compile_definitions(mem_none_folklore PRIVATE -D GROWT_RSS_MODE)
  GrowTExecutable( UAGROW ins_test mem mem_full_uaGrowT )
  target_compile_definitions(mem_full_uaGrowT PRIVATE -D GROWT_RSS_MODE)
  GrowTExecutable( USGROW ins_test mem mem_full_usGrowT )
  target_compile_definitions(mem_full_usGrowT PRIVATE -D GROWT_RSS_MODE)
endif()


add_custom_target( folklore )
add_dependencies( folklore
  ins_none_folklore mix_none_folklore con_none_folklore
  agg_none_folklore)

add_custom_target( sequential )
add_dependencies( sequential
  ins_full_sequential mix_full_sequential con_full_sequential
  agg_full_sequential)

add_custom_target( uagrow )
add_dependencies( uagrow
  ins_full_uaGrowT mix_full_uaGrowT con_full_uaGrowT
  agg_full_uaGrowT del_full_uaGrowT)

add_custom_target( usgrow )
add_dependencies( usgrow
  ins_full_usGrowT mix_full_usGrowT con_full_usGrowT
  agg_full_usGrowT del_full_usGrowT)

add_custom_target( pagrow )
add_dependencies( pagrow
  ins_full_paGrowT mix_full_paGrowT con_full_paGrowT
  agg_full_paGrowT del_full_paGrowT)

add_custom_target( psgrow )
add_dependencies( psgrow
  ins_full_psGrowT mix_full_psGrowT con_full_psGrowT
  agg_full_psGrowT del_full_psGrowT)

add_custom_target( functionality )
add_dependencies ( functionality
  functionality_uaGrowT
  functionality_usGrowT
  functionality_paGrowT
  functionality_psGrowT)

add_custom_target( ins )
add_dependencies( ins
  ins_full_sequential
  ins_none_folklore
  ins_full_uaGrowT    ins_full_usGrowT
  ins_full_uaGrowT    ins_full_usGrowT)

add_custom_target( agg )
add_dependencies( agg
  agg_full_sequential
  agg_none_folklore
  agg_full_uaGrowT    agg_full_usGrowT
  agg_full_uaGrowT    agg_full_usGrowT)

add_custom_target( mix )
add_dependencies( mix
  mix_full_sequential
  mix_none_folklore
  mix_full_uaGrowT    mix_full_usGrowT
  mix_full_uaGrowT    mix_full_usGrowT)

add_custom_target( con )
add_dependencies( con
  con_full_sequential
  con_none_folklore
  con_full_uaGrowT    con_full_usGrowT
  con_full_uaGrowT    con_full_usGrowT)

add_custom_target( del )
add_dependencies( del
  del_full_uaGrowT    del_full_usGrowT
  del_full_uaGrowT    del_full_usGrowT)

if (GROWT_BUILD_CIRCULAR)
  GrowTExecutable( UAGROW ins_test ins ins_full_cmuaGrowT )
  target_compile_definitions(ins_full_cmuaGrowT PRIVATE
    -D CMAP)
  GrowTExecutable( USGROW ins_test ins ins_full_cmusGrowT )
  target_compile_definitions(ins_full_cmusGrowT PRIVATE
    -D CMAP)
  GrowTExecutable( UAGROW ins_test ins ins_full_cpuaGrowT )
  target_compile_definitions(ins_full_cpuaGrowT PRIVATE
    -D CPROB)
  GrowTExecutable( USGROW ins_test ins ins_full_cpusGrowT )
  target_compile_definitions(ins_full_cpusGrowT PRIVATE
    -D CPROB)
  GrowTExecutable( UAGROW ins_test ins ins_full_ccuaGrowT )
  target_compile_definitions(ins_full_ccuaGrowT PRIVATE
    -D CMAP -D CPROB)
  GrowTExecutable( USGROW ins_test ins ins_full_ccusGrowT )
  target_compile_definitions(ins_full_ccusGrowT PRIVATE
    -D CMAP -D CPROB)
endif()
