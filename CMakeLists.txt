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

option(GROWT_BUILD_TSX
  "(optional) builds tests using TSX optimized hash tables." OFF)

option(GROWT_BUILD_OLD_VARIANTS
  "(optional) builds older legacy variants of our asynchroneously growing hash tables." OFF)

option(GROWT_BUILD_CIRCULAR
  "(optional) builds variants with other mapping/probing methods." OFF)

option(GROWT_BUILD_ALL_THIRD_PARTIES
  "(optional) builds tests for third party hash tables." OFF)

option(GROWT_BUILD_FOLLY
  "(optional) builds tests for folly (using a wrapper)!" OFF)
option(GROWT_BUILD_CUCKOO
  "(optional) builds tests for libcuckoo (using a wrapper)!" OFF)
option(GROWT_BUILD_TBB
  "(optional) builds tests for tbb hash map and unordered map (using wrappers)!" OFF)
option(GROWT_BUILD_JUNCTION
  "(optional) builds tests for junction tables (Linear, Grampa, Leapfrog)!" OFF)
option(GROWT_BUILD_MEMORY_TEST
  "(optional) builds tests that output memory (i.e. residential set size)" OFF)
set(GROWT_USE_SCALABLE_ALLOCATORS_WHERE_APPROPRIATE
  "(optional) text test and complex table use TBB::scalable allocator by default" ON)

set(GROWT_ALLOCATOR ALIGNED CACHE STRING
  "Specifies the used allocator (only relevant for our tables)!")
set_property(CACHE GROWT_ALLOCATOR PROPERTY STRINGS ALIGNED POOL TBB_ALIGNED NUMA_POOL HTLB_POOL)

set(GROWT_ALLOCATOR_POOL_SIZE 2 CACHE STRING
  "Size of preallocated memory pool (only relevant for pool allocators)!")
if (NOT GROWT_ALLOCATOR_POOL_SIZE MATCHES "^[0-9]+$")
  message(FATAL_ERROR "GROWT_ALLOCATOR_POOL_SIZE must be a numeric argument")
endif()

set(GROWT_HASHFCT XXH3 CACHE STRING
  "Changes the used hash function if XXHASH is not available, MURMUR2 is used as backoff!")
set_property(CACHE GROWT_HASHFCT PROPERTY STRINGS XXH3 XXHASH MURMUR2 MURMUR3 CRC)

if (GROWT_BUILD_ALL_THIRD_PARTIES)
  set(GROWT_BUILD_FOLLY    ON)
  set(GROWT_BUILD_CUCKOO   ON)
  set(GROWT_BUILD_TBB      ON)
  set(GROWT_BUILD_JUNCTION ON)
endif()

###TEMPORARY (CURRENTLY NOT USED)
set (GROWT_MAX_FILL 0.6)

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

if (GROWT_ALLOCATOR STREQUAL POOL OR
    GROWT_ALLOCATOR STREQUAL NUMA_POOL OR
    GROWT_ALLOCATOR STREQUAL HTLB_POOL OR
    GROWT_ALLOCATOR STREQUAL TBB_ALIGNED)
  set (USE_TBB_MEMPOOL ON)
endif()



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

message(STATUS "Looking for Intel TBB.")
find_package(TBB)
if (NOT TBB_FOUND)
  message(STATUS "Looking for Intel TBB -- not found. "
    "memPool cannot be used and TBB-Tests cannot be created!")
  set(GROWT_BUILD_TBB OFF)
  if (USE_TBB_MEMPOOL)
    message(STATUS "Switch to aligned alloc!")
    set(GROWT_ALLOCATOR ALIGNED)
  endif()
  if (GROWT_USE_SCALABLE_ALLOCATORS_WHERE_APPROPRIATE)
    message(STATUS "Switch string and complex_slot allocators to default!")
    set(GROWT_USE_SCALABLE_ALLOCATORS_WHERE_APPROPRIATE OFF)
  endif()
elseif(TBB_INTERFACE_VERSION LESS 8000)
  message("Intel TBB ${TBB_INTERFACE_VERSION} is too old. "
    "ThreadPool cannot be used and TBB-Tests cannot be created!")
  set(GROWT_BUILD_TBB OFF)
  if (USE_TBB_MEMPOOL)
    message(STATUS "Switch to aligned alloc!")
    set(GROWT_ALLOCATOR ALIGNED)
  endif()
  if (GROWT_USE_SCALABLE_ALLOCATORS_WHERE_APPROPRIATE)
    message(STATUS "Switch string and complex_slot allocators to default!")
    set(GROWT_USE_SCALABLE_ALLOCATORS_WHERE_APPROPRIATE OFF)
  endif()
else()
  message(STATUS "Looking for Intel TBB. -- found")
endif()

if (GROWT_ALLOCATOR STREQUAL NUMA_POOL)
  find_package(NUMA)
  if (NOT NUMA_FOUND)
    message("Cannot find libnuma. "
      "Therefore, a normal pool allocator will be used!")
    set(GROWT_ALLOCATOR POOL)
  else()
    include_directories(SYSTEM &{NUMA_INCLUDE_DIRS})
    #message(STATUS "${NUMA_LIBRARIES}")
    link_directories(${NUMA_LIBRARY_DIRS})
    set(ALLOC_LIB ${ALLOC_LIB} ${NUMA_LIBRARIES})
  endif()
endif()


##### SUBMODULE FINDS ##########################################################

if (GROWT_BUILD_CUCKOO)
  message(STATUS "Looking for libcuckoo.")

  find_path(CUCKOO_DIR libcuckoo/CMakeLists.txt
    PATHS HINTS ${CMAKE_CURRENT_SOURCE_DIR}/misc/submodules)
      #PATHS ENV PATH ENV CUCKOO_ROOT)

  if (CUCKOO_DIR)
    add_subdirectory(${CUCKOO_DIR}/libcuckoo libcuckoo)
    message(STATUS ${CUCKOO_DIR})
    include_directories(${CUCKOO_DIR}/libcuckoo)
    #set(JUNCTIONLIST "junction_linear;junction_grampa;junction_leapfrog")
    message(STATUS "Looking for libcuckoo -- found")
  else()
    message(STATUS "Looking for libcuckoo -- not found")
    message("Cannot find libcuckoo root directory. "
      "Therefore, Cuckoo-Tests cannot be created!")
    set(GROWT_BUILD_CUCKOO OFF)
  endif()
endif()

if (GROWT_BUILD_JUNCTION)
  message(STATUS "Looking for junction")

  find_path(JUNCTION_DIR junction/CMakeLists.txt
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/misc/submodules)
      #PATHS ENV PATH ENV JUNCTION_ROOT)

  if (JUNCTION_DIR)
    add_subdirectory(${JUNCTION_DIR}/junction junction)
    include_directories(${JUNCTION_ALL_INCLUDE_DIRS})
    #set(JUNCTIONLIST "junction_linear;junction_grampa;junction_leapfrog")
    message(STATUS "Looking for junction -- found")
  else()
    message(STATUS "Looking for junction -- not found")
    message("Cannot find junction root directory. "
      "Therefore, Junction-Tests cannot be created!")
    set(GROWT_BUILD_JUNCTION OFF)
  endif()
endif()

if (GROWT_BUILD_FOLLY)
  message(STATUS "Looking for folly")

  # NECESSARY FOR SOME REASON
  include(CheckIncludeFileCXX)

  find_path(FOLLY_DIR folly/CMakeLists.txt
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/misc/submodules)
      #PATHS ENV PATH ENV JUNCTION_ROOT)

  if (FOLLY_DIR)
    add_subdirectory(${FOLLY_DIR}/folly folly)
    include_directories(${FOLLY_DIR}/folly)
    message(STATUS "Looking for folly -- found")
  else()
    message(STATUS "Looking for folly -- not found")
    message("Cannot find folly root directory. "
      "Therefore, Folly-Tests cannot be created!")
    set(GROWT_BUILD_FOLLY OFF)
  endif()
endif()


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

configure_file ("misc/growt_config.h.in" "${PROJECT_BINARY_DIR}/growt_config.h")
include_directories(${PROJECT_BINARY_DIR})

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

if (GROWT_BUILD_TBB AND TBB_FOUND)
  GrowTExecutable( FOLKLORE text_test text text_none_folklore )
  GrowTExecutable( SEQUENTIAL text_test text text_full_sequential )
  GrowTExecutable( UAGROW text_test text text_full_uaGrowT )
  GrowTExecutable( USGROW text_test text text_full_usGrowT )
  GrowTExecutable( PAGROW text_test text text_full_paGrowT )
  GrowTExecutable( PSGROW text_test text text_full_psGrowT )
  # target_link_libraries(text_none_folklore ${TBB_LIBRARIES})
  # target_link_libraries(text_full_sequential ${TBB_LIBRARIES})
  # target_link_libraries(text_full_uaGrowT ${TBB_LIBRARIES})
  # target_link_libraries(text_full_usGrowT ${TBB_LIBRARIES})
  # target_link_libraries(text_full_paGrowT ${TBB_LIBRARIES})
  # target_link_libraries(text_full_psGrowT ${TBB_LIBRARIES})
  # target_link_libraries(functionality_uaGrowT ${TBB_LIBRARIES})
  # target_link_libraries(functionality_usGrowT ${TBB_LIBRARIES})
  # target_link_libraries(functionality_paGrowT ${TBB_LIBRARIES})
  # target_link_libraries(functionality_psGrowT ${TBB_LIBRARIES})

  if (GROWT_USE_SCALABLE_ALLOCATORS_WHERE_APPROPRIATE)
    target_compile_definitions(text_none_folklore    PRIVATE -D TBB_AS_DEFAULT)
    target_compile_definitions(text_full_sequential  PRIVATE -D TBB_AS_DEFAULT)
    target_compile_definitions(text_full_uaGrowT     PRIVATE -D TBB_AS_DEFAULT)
    target_compile_definitions(text_full_usGrowT     PRIVATE -D TBB_AS_DEFAULT)
    target_compile_definitions(text_full_paGrowT     PRIVATE -D TBB_AS_DEFAULT)
    target_compile_definitions(text_full_psGrowT     PRIVATE -D TBB_AS_DEFAULT)
    target_compile_definitions(functionality_uaGrowT PRIVATE -D TBB_AS_DEFAULT)
    target_compile_definitions(functionality_usGrowT PRIVATE -D TBB_AS_DEFAULT)
    target_compile_definitions(functionality_paGrowT PRIVATE -D TBB_AS_DEFAULT)
    target_compile_definitions(functionality_psGrowT PRIVATE -D TBB_AS_DEFAULT)
    target_link_libraries(text_none_folklore    PRIVATE TBB::tbbmalloc)
    target_link_libraries(text_full_sequential  PRIVATE TBB::tbbmalloc)
    target_link_libraries(text_full_uaGrowT     PRIVATE TBB::tbbmalloc)
    target_link_libraries(text_full_usGrowT     PRIVATE TBB::tbbmalloc)
    target_link_libraries(text_full_paGrowT     PRIVATE TBB::tbbmalloc)
    target_link_libraries(text_full_psGrowT     PRIVATE TBB::tbbmalloc)
    target_link_libraries(functionality_uaGrowT PRIVATE TBB::tbbmalloc)
    target_link_libraries(functionality_usGrowT PRIVATE TBB::tbbmalloc)
    target_link_libraries(functionality_paGrowT PRIVATE TBB::tbbmalloc)
    target_link_libraries(functionality_psGrowT PRIVATE TBB::tbbmalloc)
  endif()


  add_custom_target( text )
  add_dependencies( text
    text_full_sequential
    text_none_folklore
    text_full_uaGrowT    text_full_usGrowT
    text_full_uaGrowT    text_full_usGrowT)
endif()

if (GROWT_BUILD_TBB)
  function( TBBExecutable variant cpp directory name )
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${directory})
    add_executable(${name} tests/${cpp}.cpp)
    set_target_properties(${name} PROPERTIES COMPILE_FLAGS "${FLAGS}")
    target_compile_definitions(${name} PRIVATE
      -D ${variant}
      -D ${GROWT_HASHFCT}
      -D ${GROWT_ALLOCATOR})
    target_link_libraries( ${name} ${TEST_DEP_LIBRARIES} PRIVATE TBB::tbb )
  endfunction( TBBExecutable )

  TBBExecutable( TBBHM  ins_test ins ins_full_TBBhm )
  TBBExecutable( TBBUM  ins_test ins ins_full_TBBum )
  TBBExecutable( TBBHM  mix_test mix mix_full_TBBhm )
  TBBExecutable( TBBUM  mix_test mix mix_full_TBBum )
  TBBExecutable( TBBHM  con_test con con_full_TBBhm )
  TBBExecutable( TBBUM  con_test con con_full_TBBum )
  TBBExecutable( TBBHM  agg_test agg agg_full_TBBhm )
  TBBExecutable( TBBUM  agg_test agg agg_full_TBBum )

  TBBExecutable( TBBUM  text_test text text_full_TBBum )
  TBBExecutable( TBBHM  text_test text text_full_TBBhm )
  target_link_libraries(text_full_TBBum PRIVATE TBB::tbb PRIVATE TBB::tbbmalloc)
  target_link_libraries(text_full_TBBhm PRIVATE TBB::tbb PRIVATE TBB::tbbmalloc)

  add_custom_target( tbb )
  add_dependencies( tbb
    ins_full_TBBhm ins_full_TBBum
    mix_full_TBBhm mix_full_TBBum
    con_full_TBBhm con_full_TBBum
    agg_full_TBBhm agg_full_TBBum)

  if (GROWT_BUILD_MEMORY_TEST)
    TBBExecutable( TBBHM ins_test mem mem_full_TBBhm )
    target_compile_definitions(mem_full_TBBhm PRIVATE -D GROWT_RSS_MODE)
    TBBExecutable( TBBUM ins_test mem mem_full_TBBum )
    target_compile_definitions(mem_full_TBBum PRIVATE -D GROWT_RSS_MODE)
  endif()
endif()

if (GROWT_BUILD_FOLLY)
  function( FollyExecutable variant cpp directory name )
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${directory})

    add_executable(${name} tests/${cpp}.cpp  )
    set_target_properties(${name} PROPERTIES COMPILE_FLAGS "${FLAGS}") #"-O2")
    target_compile_definitions(${name} PRIVATE
      -D ${variant}
      -D ${GROWT_HASHFCT}
      -D ${GROWT_ALLOCATOR})
    target_link_libraries( ${name} ${TEST_DEP_LIBRARIES} folly libglog.so )
  endfunction( FollyExecutable )

  FollyExecutable( FOLLY  ins_test ins ins_semi_folly )
  FollyExecutable( FOLLY  mix_test mix mix_semi_folly )
  FollyExecutable( FOLLY  con_test con con_semi_folly )
  FollyExecutable( FOLLY  agg_test agg agg_semi_folly )

  add_custom_target( follytrgt )
  add_dependencies( follytrgt
    ins_semi_folly mix_semi_folly con_semi_folly
    agg_semi_folly)

  if (GROWT_BUILD_MEMORY_TEST)
    FollyExecutable( FOLLY ins_test mem mem_semi_folly )
    target_compile_definitions(mem_semi_folly PRIVATE -D GROWT_RSS_MODE)
  endif()
endif()

if (GROWT_BUILD_CUCKOO)
  function( CuckooExecutable variant cpp directory name )
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${directory})
    add_executable(${name} tests/${cpp}.cpp)
    set_target_properties(${name} PROPERTIES COMPILE_FLAGS "${FLAGS}")
    target_compile_definitions(${name} PRIVATE
      -D ${variant}
      -D ${GROWT_HASHFCT}
      -D ${GROWT_ALLOCATOR})
    target_link_libraries( ${name} ${TEST_DEP_LIBRARIES} ${CUCKOO_LIBRARIES} )
  endfunction( CuckooExecutable )

  CuckooExecutable( CUCKOO  ins_test ins ins_full_cuckoo )
  CuckooExecutable( CUCKOO  mix_test mix mix_full_cuckoo )
  CuckooExecutable( CUCKOO  con_test con con_full_cuckoo )
  CuckooExecutable( CUCKOO  agg_test agg agg_full_cuckoo )
  CuckooExecutable( CUCKOO  del_test del del_full_cuckoo )
  if (TBB_FOUND)
    CuckooExecutable( CUCKOO  text_test text text_full_cuckoo )
    target_link_libraries(text_full_cuckoo PRIVATE TBB::tbb)
  endif()

  add_custom_target( cuckoo )
  add_dependencies( cuckoo
    ins_full_cuckoo mix_full_cuckoo con_full_cuckoo
    agg_full_cuckoo del_full_cuckoo)

  if (GROWT_BUILD_MEMORY_TEST)
    CuckooExecutable( CUCKOO ins_test mem mem_full_cuckoo )
    target_compile_definitions(mem_full_cuckoo PRIVATE -D GROWT_RSS_MODE)
  endif()
endif()

if (GROWT_BUILD_JUNCTION)
  function( JunctionExecutable variant cpp directory name )
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${directory})
    add_executable(${name} tests/${cpp}.cpp)
    set_target_properties(${name} PROPERTIES COMPILE_FLAGS "${FLAGS}")
    target_compile_definitions(${name} PRIVATE
      -D ${variant}
      -D ${GROWT_HASHFCT}
      -D ${GROWT_ALLOCATOR})
    target_link_libraries( ${name} ${TEST_DEP_LIBRARIES} ${JUNCTION_ALL_LIBRARIES} )
  endfunction( JunctionExecutable )

  JunctionExecutable( JUNCTION_LINEAR      ins_test ins ins_full_junction_linear )
  JunctionExecutable( JUNCTION_LINEAR      mix_test mix mix_full_junction_linear )
  JunctionExecutable( JUNCTION_LINEAR      con_test con con_full_junction_linear )
  JunctionExecutable( JUNCTION_LINEAR      del_test del del_full_junction_linear )
  JunctionExecutable( JUNCTION_GRAMPA      ins_test ins ins_full_junction_grampa )
  JunctionExecutable( JUNCTION_GRAMPA      mix_test mix mix_full_junction_grampa )
  JunctionExecutable( JUNCTION_GRAMPA      con_test con con_full_junction_grampa )
  JunctionExecutable( JUNCTION_GRAMPA      del_test del del_full_junction_grampa )
  JunctionExecutable( JUNCTION_LEAPFROG    ins_test ins ins_full_junction_leap )
  JunctionExecutable( JUNCTION_LEAPFROG    mix_test mix mix_full_junction_leap )
  JunctionExecutable( JUNCTION_LEAPFROG    con_test con con_full_junction_leap )
  JunctionExecutable( JUNCTION_LEAPFROG    del_test del del_full_junction_leap )

  add_custom_target( junctiontrgt )
  add_dependencies( junctiontrgt
    ins_full_junction_linear ins_full_junction_grampa ins_full_junction_leap
    mix_full_junction_linear mix_full_junction_grampa mix_full_junction_leap
    con_full_junction_linear con_full_junction_grampa con_full_junction_leap
    del_full_junction_linear del_full_junction_grampa del_full_junction_leap)

  if (GROWT_BUILD_MEMORY_TEST)
    JunctionExecutable( JUNCTION_LINEAR   ins_test mem mem_full_junction_linear )
    target_compile_definitions(mem_full_junction_linear PRIVATE -D GROWT_RSS_MODE)
    JunctionExecutable( JUNCTION_GRAMPA   ins_test mem mem_full_junction_grampa )
    target_compile_definitions(mem_full_junction_grampa PRIVATE -D GROWT_RSS_MODE)
    JunctionExecutable( JUNCTION_LEAPFROG ins_test mem mem_full_junction_leap )
    target_compile_definitions(mem_full_junction_leap   PRIVATE -D GROWT_RSS_MODE)
  endif()
endif()

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
