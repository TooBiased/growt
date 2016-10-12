################################################################################
# misc/cmake/Findsmhasher.cmake
#
# Finds smhasher directory
# Looks in PATH and SMHASHER_ROOT environment variables
#
# Part of Project growt - https://github.com/TooBiased/growt.git
#
# Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
#
# All rights reserved. Published under the BSD-2 license in the LICENSE file.
################################################################################

message(STATUS "Looking for smhasher Implementation")

find_path(SMHASHER_PATH smhasher/src/MurmurHash3.cpp
  PATHS ENV PATH ENV SMHASHER_ROOT)

if(SMHASHER_PATH)
  message(STATUS "Looking for smhasher - found path: ${SMHASHER_PATH}")
  set(SMHASHER_FOUND ON)

  set(SMHASHER_INCLUDE_DIRS "${SMHASHER_PATH}/smhasher/src/")
else()
  message(STATUS "failed finding smhasher - please create the SMHASHER_ROOT environment variable or append it to PATH")
  if (smhasher_FIND_REQUIRED)
    message(FATAL_ERROR "Required package smhasher missing!")
  endif()
endif()
