################################################################################
# misc/cmake/FindXXHash.cmake
#
# Finds xxHash directory
# Looks in PATH and XXHASH_ROOT environment variables
#
# Part of Project growt - https://github.com/TooBiased/growt.git
#
# Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
#
# All rights reserved. Published under the BSD-2 license in the LICENSE file.
################################################################################

message(STATUS "Looking for xxHash Implementation")

find_path(XXHASH_PATH xxHash/xxhash.h
  PATHS ENV PATH ENV XXHASH_ROOT)

if(XXHASH_PATH)
  message(STATUS "Looking for xxHash - found path: ${XXHASH_PATH}")
  set(XXHASH_FOUND ON)

  set(XXHASH_INCLUDE_DIRS "${XXHASH_PATH}/xxHash/")
else()
  message(STATUS "failed finding xxHash - please create the XXHASH_ROOT environment variable or append it to PATH")
  if (XXHash_FIND_REQUIRED)
    message(FATAL_ERROR "Required package xxHash missing!")
  endif()
endif()
