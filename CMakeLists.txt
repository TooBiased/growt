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

cmake_minimum_required (VERSION 2.8)

# custom cmake scripts
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/misc/cmake)

project (concurrent-growtable)


# prohibit in-source builds
if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
  message(SEND_ERROR "In-source builds are not allowed.")
endif()

option(GROWT_BUILD_TSX
  "(optional) builds tests using TSX optimized hash tables." OFF)

option(GTOWT_BUILD_ALTERNATE_VARIANT
  "(optional) builds another variant of our synchroneously growing hash tables (usnGrow and psnGrow)." OFF)

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

set(GROWT_ALLOCATOR ALIGNED CACHE STRING
  "Specifies the used allocator (only relevant for our tables)!")
set_property(CACHE GROWT_ALLOCATOR PROPERTY STRINGS ALIGNED POOL NUMA_POOL HTLB_POOL)

set(GROWT_ALLOCATOR_POOL_SIZE 2 CACHE STRING
  "Size of preallocated memory pool (only relevant for pool allocators)!")
if (NOT GROWT_ALLOCATOR_POOL_SIZE MATCHES "^[0-9]+$")
  message(FATAL_ERROR "GROWT_ALLOCATOR_POOL_SIZE must be a numeric argument")
endif()

set(GROWT_HASHFCT XXHASH CACHE STRING
  "Changes the used hash function if XXHASH is not available, MURMUR2 is used as backoff!")
set_property(CACHE GROWT_HASHFCT PROPERTY STRINGS XXHASH MURMUR2 MURMUR3 CRC)

if (GROWT_BUILD_ALL_THIRD_PARTIES)
  set(GROWT_BUILD_FOLLY    ON)
  set(GROWT_BUILD_CUCKOO   ON)
  set(GROWT_BUILD_TBB      ON)
  set(GROWT_BUILD_JUNCTION ON)
endif()

###TEMPORARY (CURRENTLY NOT USED)
set (GROWT_MAX_FILL 0.6)


set (OLD_FLAGS ${CMAKE_CXX_FLAGS})

# set (CMAKE_CXX_FLAGS
#   "${CMAKE_CXX_FLAGS} -std=c++1y -g -msse4.2 -mcx16 -Wall -Wextra -O3")

set (FLAGS "-std=c++1y -g -msse4.2 -mcx16 -Wall -Wextra -O3")

include_directories(.)

if (GROWT_ALLOCATOR STREQUAL POOL OR
    GROWT_ALLOCATOR STREQUAL NUMA_POOL OR
    GROWT_ALLOCATOR STREQUAL HTLB_POOL)
  set (USE_TBB_MEMPOOL ON)
endif()



# find pthread
find_package(Threads REQUIRED)
set(TEST_DEP_LIBRARIES ${CMAKE_THREAD_LIBS_INIT} ${TEST_DEP_LIBRARIES})



# COMPILE THE EXAMPLE CODE (example/example.cpp)
add_executable(example example/example.cpp)
set_target_properties(example PROPERTIES COMPILE_FLAGS "${FLAGS}")
target_link_libraries(example ${TEST_DEP_LIBRARIES})

add_executable(range_example example/range_example.cpp)
set_target_properties(range_example PROPERTIES COMPILE_FLAGS "${FLAGS}")
target_link_libraries(range_example ${TEST_DEP_LIBRARIES})


# if necessary find tbb
if (USE_TBB_MEMPOOL OR GROWT_BUILD_TBB)
  find_package(TBB)

  if (NOT TBB_FOUND)
    message("Intel TBB not found. "
      "memPool cannot be used and TBB-Tests cannot be created!")
    set(GROWT_BUILD_TBB OFF)
    if (USE_TBB_MEMPOOL)
      message(STATUS "Switch to aligned alloc!")
      set(GROWT_ALLOCATOR ALIGNED)
    endif()
  elseif(TBB_INTERFACE_VERSION LESS 8000)
    message("Intel TBB ${TBB_INTERFACE_VERSION} is too old. "
      "ThreadPool cannot be used and TBB-Tests cannot be created!")
    set(GROWT_BUILD_TBB OFF)
    if (USE_TBB_MEMPOOL)
      message(STATUS "Switch to aligned alloc!")
      set(GROWT_ALLOCATOR ALIGNED)
    endif()
  else()
    include_directories(STATUS ${TBB_INCLUDE_DIRS})
    link_directories(${TBB_LIBRARY_DIRS})
    if (USE_TBB_MEMPOOL)
      set(ALLOC_LIB ${TBB_LIBRARIES})
    endif()
  endif()
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


if (GROWT_HASHFCT STREQUAL XXHASH)
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
  target_link_libraries(${name} ${TEST_DEP_LIBRARIES} ${ALLOC_LIB})
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
  target_link_libraries(${name} ${TEST_DEP_LIBRARIES} ${ALLOC_LIB})
endfunction( GrowXExecutable )


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

if (GTOWT_BUILD_ALTERNATE_VARIANT)
  GrowTExecutable( USNGROW ins_test ins ins_full_usnGrowT )
  GrowTExecutable( PSNGROW ins_test ins ins_full_psnGrowT )
  GrowTExecutable( USNGROW mix_test mix mix_full_usnGrowT )
  GrowTExecutable( PSNGROW mix_test mix mix_full_psnGrowT )
  GrowTExecutable( USNGROW del_test del del_full_usnGrowT )
  GrowTExecutable( PSNGROW del_test del del_full_psnGrowT )
  GrowTExecutable( USNGROW con_test con con_full_usnGrowT )
  GrowTExecutable( PSNGROW con_test con con_full_psnGrowT )
  GrowTExecutable( USNGROW agg_test agg agg_full_usnGrowT )
  GrowTExecutable( PSNGROW agg_test agg agg_full_psnGrowT )
endif()

if (GROWT_BUILD_TSX)
  GrowXExecutable( XFOLKLORE ins_test ins ins_none_xfolklore )
  GrowXExecutable( XFOLKLORE mix_test mix mix_none_xfolklore )
  GrowXExecutable( XFOLKLORE con_test con con_none_xfolklore )
  GrowXExecutable( XFOLKLORE agg_test agg agg_none_xfolklore )
  GrowXExecutable( UAXGROW ins_test ins ins_full_uaxGrowT )
  GrowXExecutable( USXGROW ins_test ins ins_full_usxGrowT )
  GrowXExecutable( PAXGROW ins_test ins ins_full_paxGrowT )
  GrowXExecutable( PSXGROW ins_test ins ins_full_psxGrowT )
  GrowXExecutable( UAXGROW mix_test mix mix_full_uaxGrowT )
  GrowXExecutable( USXGROW mix_test mix mix_full_usxGrowT )
  GrowXExecutable( PAXGROW mix_test mix mix_full_paxGrowT )
  GrowXExecutable( PSXGROW mix_test mix mix_full_psxGrowT )
  GrowXExecutable( UAXGROW del_test del del_full_uaxGrowT )
  GrowXExecutable( USXGROW del_test del del_full_usxGrowT )
  GrowXExecutable( PAXGROW del_test del del_full_paxGrowT )
  GrowXExecutable( PSXGROW del_test del del_full_psxGrowT )
  GrowXExecutable( UAXGROW con_test con con_full_uaxGrowT )
  GrowXExecutable( USXGROW con_test con con_full_usxGrowT )
  GrowXExecutable( PAXGROW con_test con con_full_paxGrowT )
  GrowXExecutable( PSXGROW con_test con con_full_psxGrowT )
  GrowXExecutable( UAXGROW agg_test agg agg_full_uaxGrowT )
  GrowXExecutable( USXGROW agg_test agg agg_full_usxGrowT )
  GrowXExecutable( PAXGROW agg_test agg agg_full_paxGrowT )
  GrowXExecutable( PSXGROW agg_test agg agg_full_psxGrowT )

  if (GTOWT_BUILD_ALTERNATE_VARIANT)
    GrowXExecutable( USNXGROW ins_test ins ins_full_usnxGrowT )
    GrowXExecutable( PSNXGROW ins_test ins ins_full_psnxGrowT )
    GrowXExecutable( USNXGROW mix_test mix mix_full_usnxGrowT )
    GrowXExecutable( PSNXGROW mix_test mix mix_full_psnxGrowT )
    GrowXExecutable( USNXGROW del_test del del_full_usnxGrowT )
    GrowXExecutable( PSNXGROW del_test del del_full_psnxGrowT )
    GrowXExecutable( USNXGROW con_test con con_full_usnxGrowT )
    GrowXExecutable( PSNXGROW con_test con con_full_psnxGrowT )
    GrowXExecutable( USNXGROW agg_test agg agg_full_usnxGrowT )
    GrowXExecutable( PSNXGROW agg_test agg agg_full_psnxGrowT )
  endif()
endif()

if (GROWT_BUILD_TBB)
  function( TBBExecutable variant cpp directory name )
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${directory})
    add_executable(${name} tests/${cpp}.cpp)
    set_target_properties(${name} PROPERTIES COMPILE_FLAGS "${FLAGS}")
    target_compile_definitions(${name} PRIVATE
      -D ${variant}
      -D ${GROWT_HASHFCT})
    target_link_libraries( ${name} ${TEST_DEP_LIBRARIES} ${TBB_LIBRARIES} )
  endfunction( TBBExecutable )

  TBBExecutable( TBBHM  ins_test ins ins_full_TBBhm )
  TBBExecutable( TBBUM  ins_test ins ins_full_TBBum )
  TBBExecutable( TBBHM  mix_test mix mix_full_TBBhm )
  TBBExecutable( TBBUM  mix_test mix mix_full_TBBum )
  TBBExecutable( TBBHM  con_test con con_full_TBBhm )
  TBBExecutable( TBBUM  con_test con con_full_TBBum )
  TBBExecutable( TBBHM  agg_test agg agg_full_TBBhm )
  TBBExecutable( TBBUM  agg_test agg agg_full_TBBum )
endif()

if (GROWT_BUILD_FOLLY)
  # set(TEMP_FLAGS "${CMAKE_CXX_FLAGS}")
  # set(CMAKE_CXX_FLAGS "${OLD_FLAGS}")

  function( FollyExecutable variant cpp directory name )
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${directory})

    add_executable(${name} tests/${cpp}.cpp  )
    set_target_properties(${name} PROPERTIES COMPILE_FLAGS "-O2")
    target_compile_definitions(${name} PRIVATE
      -D ${variant}
      -D ${GROWT_HASHFCT})
    target_link_libraries( ${name} ${TEST_DEP_LIBRARIES} folly libglog.so )
  endfunction( FollyExecutable )

  FollyExecutable( FOLLY  ins_test ins ins_semi_folly )
  FollyExecutable( FOLLY  mix_test mix mix_semi_folly )
  FollyExecutable( FOLLY  con_test con con_semi_folly )
  FollyExecutable( FOLLY  agg_test agg agg_semi_folly )

  # set(CMAKE_CXX_FLAGS "${TEMP_FLAGS}")
endif()

if (GROWT_BUILD_CUCKOO)
  function( CuckooExecutable variant cpp directory name )
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${directory})
    add_executable(${name} tests/${cpp}.cpp)
    set_target_properties(${name} PROPERTIES COMPILE_FLAGS "${FLAGS}")
    target_compile_definitions(${name} PRIVATE
      -D ${variant}
      -D ${GROWT_HASHFCT})
    target_link_libraries( ${name} ${TEST_DEP_LIBRARIES} ${CUCKOO_LIBRARIES} )
  endfunction( CuckooExecutable )

  CuckooExecutable( CUCKOO  ins_test ins ins_full_cuckoo )
  CuckooExecutable( CUCKOO  mix_test mix mix_full_cuckoo )
  CuckooExecutable( CUCKOO  con_test con con_full_cuckoo )
  CuckooExecutable( CUCKOO  agg_test agg agg_full_cuckoo )
  CuckooExecutable( CUCKOO  del_test del del_full_cuckoo )
endif()

if (GROWT_BUILD_JUNCTION)
  function( JunctionExecutable variant cpp directory name )
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${directory})
    add_executable(${name} tests/${cpp}.cpp)
    set_target_properties(${name} PROPERTIES COMPILE_FLAGS "${FLAGS}")
    target_compile_definitions(${name} PRIVATE
      -D ${variant}
      -D ${GROWT_HASHFCT})
    target_link_libraries( ${name} ${TEST_DEP_LIBRARIES} ${JUNCTION_ALL_LIBRARIES} )
  endfunction( JunctionExecutable )

  JunctionExecutable( JUNCTION_LINEAR  ins_test ins ins_full_junction_linear )
  JunctionExecutable( JUNCTION_LINEAR  mix_test mix mix_full_junction_linear )
  JunctionExecutable( JUNCTION_LINEAR  con_test con con_full_junction_linear )
  JunctionExecutable( JUNCTION_LINEAR  agg_test agg agg_full_junction_linear )
  JunctionExecutable( JUNCTION_LINEAR  del_test del del_full_junction_linear )
  JunctionExecutable( JUNCTION_GRAMPA  ins_test ins ins_full_junction_grampa )
  JunctionExecutable( JUNCTION_GRAMPA  mix_test mix mix_full_junction_grampa )
  JunctionExecutable( JUNCTION_GRAMPA  con_test con con_full_junction_grampa )
  JunctionExecutable( JUNCTION_GRAMPA  agg_test agg agg_full_junction_grampa )
  JunctionExecutable( JUNCTION_GRAMPA  del_test del del_full_junction_grampa )
  JunctionExecutable( JUNCTION_LEAPFROG    ins_test ins ins_full_junction_leap )
  JunctionExecutable( JUNCTION_LEAPFROG    mix_test mix mix_full_junction_leap )
  JunctionExecutable( JUNCTION_LEAPFROG    con_test con con_full_junction_leap )
  JunctionExecutable( JUNCTION_LEAPFROG    agg_test agg agg_full_junction_leap )
  JunctionExecutable( JUNCTION_LEAPFROG    del_test del del_full_junction_leap )
endif()



# # build additional tables when tsx is enabled
# if (GROWT_BUILD_TSX)
#   set (HASHLIST "${HASHLIST};xfolklore;uaxGrow;usxGrow;paxGrow;psxGrow")
#   set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mrtm")
#   if (GROWT_BUILD_ALTERNATE_VARIANT)
#     set(HASHLIST "${HASHLIST};usnxGrow;psnxGrow")
#   endif()
# endif()

# if (GROWT_HASHFCT STREQUAL MURMUR3)
#   find_package(smhasher)
#   if (SMHASHER_FOUND)
#     include_directories(${SMHASHER_INCLUDE_DIRS})
#   else()
#     message("Cannot find SMHasher which implements MURMUR3. Use MURMUR2 instead.")
#   endif()
# endif()
