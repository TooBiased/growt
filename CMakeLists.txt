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
  "(optional) builds tests for third party hash tables." ON)

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


set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y -msse4.2 -mcx16 -Wall -Wextra -O3 -g")
include_directories(.)
set(HASHLIST "sequential;folklore;deambase;deamgrow;uaGrow;usGrow;paGrow;psGrow")

if (GTOWT_BUILD_ALTERNATE_VARIANT)
  set(HASHLIST "${HASHLIST};usnGrow;psnGrow")
endif()

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
target_link_libraries(example ${TEST_DEP_LIBRARIES})



# build additional tables when tsx is enabled
if (GROWT_BUILD_TSX)
  set (HASHLIST "${HASHLIST};xfolklore;uaxGrow;usxGrow;paxGrow;psxGrow")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mrtm")
  if (GROWT_BUILD_ALTERNATE_VARIANT)
    set(HASHLIST "${HASHLIST};usnxGrow;psnxGrow")
  endif()
endif()



#### FIND APPROPRIATE THIRD PARTY LIBRARIES (non-submodules) ###################
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

# if (GROWT_BUILD_CUCKOO)
#   find_package(Cuckoo)
#   if (NOT CUCKOO_FOUND)
#     message("Cannot find libcuckoo. "
#       "Therefore, Cuckoo-Tests cannot be created!")
#     set(GROWT_BUILD_CUCKOO OFF)
#   else()
#     include_directories(SYSTEM ${CUCKOO_INCLUDE_DIRS})
#   endif()
# endif()

if(GROWT_BUILD_FOLLY)
  find_package(Folly)
  if (FOLLY_FOUND)
    include_directories(${FOLLY_INCLUDE_DIRS})
    link_directories(${FOLLY_LIBRARY_DIRS})
    set(folly_lib ${FOLLY_LIBRARIES})
  else()
    message("Cannot find Folly. "
      "Therefore, Folly-Tests cannot be created!")
    set(GROWT_BUILD_FOLLY OFF)
  endif()
endif()


##### SUBMODULE FINDS ##########################################################

if (GROWT_BUILD_CUCKOO)
  message(STATUS "Looking for libcuckoo.")
  find_path(CUCKOO_DIR libcuckoo/CMakeLists.txt
    PATHS HINTS ${CMAKE_CURRENT_SOURCE_DIR}/submodules) #ENV PATH ENV CUCKOO_ROOT)
    message(STATUS ${CMAKE_CURRENT_SOURCE_DIR})
    message(STATUS ${CUCKOO_DIR})
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
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/submodules) #ENV PATH ENV JUNCTION_ROOT)
  if (JUNCTION_DIR)
    add_subdirectory(${JUNCTION_DIR}/junction junction)
    include_directories(${JUNCTION_ALL_INCLUDE_DIRS})
    set(JUNCTIONLIST "junction_linear;junction_grampa;junction_leapfrog")
    message(STATUS "Looking for junction -- found")
  else()
    message(STATUS "Looking for junction -- not found")
    message("Cannot find junction root directory. "
      "Therefore, Junction-Tests cannot be created!")
    set(GROWT_BUILD_JUNCTION OFF)
  endif()
endif()

# if (GROWT_HASHFCT STREQUAL XXHASH)
#   find_package(XXHash)
#   if (XXHASH_FOUND)
#     include_directories(${XXHASH_INCLUDE_DIRS})
#   else()
#     message("Cannot find xxHash. Use MURMUR2 instead.")
#     set(GROWT_HASHFCT MURMUR2)
#   endif()
# endif()

if (GROWT_HASHFCT STREQUAL XXHASH)
  message(STATUS "Looking for xxhash")
  find_path(XXHASH_DIR xxHash/xxhash.h
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/submodules) #ENV ${CMAKE_CURRENT_SOURCE_DIR}/submodules) #ENV PATH ENV JUNCTION_ROOT)
  if (XXHASH_DIR)
    #add_subdirectory(${XXHASH_DIR}/xxHash xxHash)
    include_directories(${XXHASH_DIR}/xxHash)
    message(STATUS "Looking for xxhash -- found")
  else()
    message(STATUS "Looking for xxhash -- not found")
    message("Cannot find xxHash. Use MURMUR2 instead.")
    set(GROWT_HASHFCT MURMUR2)
  endif()
endif()

# if (GROWT_HASHFCT STREQUAL MURMUR3)
#   find_package(smhasher)
#   if (SMHASHER_FOUND)
#     include_directories(${SMHASHER_INCLUDE_DIRS})
#   else()
#     message("Cannot find SMHasher which implements MURMUR3. Use MURMUR2 instead.")
#   endif()
# endif()

if (GROWT_HASHFCT STREQUAL MURMUR3)
  message(STATUS "Looking for smhasher")
  find_path(MURMUR3_DIR smhasher/src/MurmurHash3.h
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/submodules) #ENV ${CMAKE_CURRENT_SOURCE_DIR}/submodules) #ENV PATH ENV JUNCTION_ROOT)
  if (MURMUR3_DIR)
    #add_subdirectory(${MURMUR3_DIR}/smhasher smhasher)
    include_directories(${MURMUR3_DIR}/smhasher)
    message(STATUS "Looking for smhasher -- found")
  else()
    message(STATUS "Looking for smhasher -- not found")
    message("Cannot find smhasher for MURMUR3. Use MURMUR2 instead.")
    set(GROWT_HASHFCT MURMUR2)
  endif()
endif()

configure_file ("misc/growt_config.h.in" "${PROJECT_BINARY_DIR}/growt_config.h")
include_directories(${PROJECT_BINARY_DIR})

#add_executable(malloc malloc_count.c)

foreach(t in agg co mix del)
  message(STATUS "create ${t}_test targets")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${t}_tests)

  foreach(h ${HASHLIST})
    string(TOUPPER ${h} hu)

    add_executable(${t}_${h} tests/${t}_test.cpp)
    target_compile_definitions(${t}_${h}
      PRIVATE -D ${hu}
      -D ${GROWT_HASHFCT}
      -D ${GROWT_ALLOCATOR}
      -D GROWT_USE_CONFIG)
    target_link_libraries(${t}_${h} ${TEST_DEP_LIBRARIES} ${ALLOC_LIB})
  endforeach()

  if (GROWT_BUILD_JUNCTION)
    foreach(h ${JUNCTIONLIST})
      string(TOUPPER ${h} hu)

      add_executable(${t}_${h} tests/${t}_test.cpp)
      target_compile_definitions(${t}_${h} PRIVATE -D ${hu}  -D ${GROWT_HASHFCT})
      target_link_libraries(${t}_${h} ${TEST_DEP_LIBRARIES} ${JUNCTION_ALL_LIBRARIES})
      set_target_properties(${t}_${h} PROPERTIES COMPILE_FLAGS -Wno-unused-value)
    endforeach()
  endif()

  if (GROWT_BUILD_TBB)
    add_executable(${t}_tbb_hm tests/${t}_test.cpp)
    target_compile_definitions(${t}_tbb_hm PRIVATE -D TBBHM  -D ${GROWT_HASHFCT})
    target_link_libraries(${t}_tbb_hm ${TEST_DEP_LIBRARIES} ${TBB_LIBRARIES})
    add_executable(${t}_tbb_um tests/${t}_test.cpp)
    target_compile_definitions(${t}_tbb_um PRIVATE -D TBBUM  -D ${GROWT_HASHFCT})
    target_link_libraries(${t}_tbb_um ${TEST_DEP_LIBRARIES} ${TBB_LIBRARIES})
  endif()

  if (GROWT_BUILD_CUCKOO)
    add_executable(${t}_cuckoo tests/${t}_test.cpp)
    target_compile_definitions(${t}_cuckoo PRIVATE -D CUCKOO -D ${GROWT_HASHFCT})
    target_link_libraries(${t}_cuckoo ${TEST_DEP_LIBRARIES})
  endif()

  if (GROWT_BUILD_FOLLY)
    add_executable(${t}_folly tests/${t}_test.cpp)
    target_compile_definitions(${t}_folly  PRIVATE -D FOLLY  -D ${GROWT_HASHFCT})
    #message(SYSTEM ${FOLLY_LIBRARIES})
    target_link_libraries(${t}_folly ${TEST_DEP_LIBRARIES} ${FOLLY_LIBRARIES} libglog.so)
  endif()

endforeach()
