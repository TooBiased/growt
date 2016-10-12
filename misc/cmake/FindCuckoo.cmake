################################################################################
# misc/cmake/FindCuckoo.cmake
#
# Finds libcuckoo installation (either correctly installed or in special folder).
# Looks in PATH and LIBCUCKOO_ROOT environment variables.
#
# Part of Project growt - https://github.com/TooBiased/growt.git
#
# Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
#
# All rights reserved. Published under the BSD-2 license in the LICENSE file.
################################################################################

message(STATUS "Looking for libcuckoo")

find_path(CUCKOO_PATH_FIRST include/libcuckoo/cuckoohash_map.hh
  PATHS ENV PATH ENV LIBCUCKOO_ROOT)


if (CUCKOO_PATH_FIRST)
  set (CUCKOO_PATH "${CUCKOO_PATH_FIRST}")
else()
  find_path(CUCKOO_PATH_SECOND libcuckoo/install/include/libcuckoo/cuckoohash_map.hh
    PATHS ENV PATH ENV LIBCUCKOO_ROOT)
  if(CUCKOO_PATH_SECOND)
    set(CUCKOO_PATH "${CUCKOO_PATH_SECOND}/libcuckoo/install/")
  endif()
endif()


if(CUCKOO_PATH)
  message(STATUS "Looking for libcuckoo - found path: ${CUCKOO_PATH}")
  set(CUCKOO_FOUND ON)

  set(CUCKOO_INCLUDE_DIRS "${CUCKOO_PATH}/include/")
else()
  message(STATUS "failed finding libcuckoo - please create the LIBCUCKOO_ROOT environment variable or append it to PATH")
  if (Cuckoo_FIND_REQUIRED)
    message(FATAL_ERROR "Required package libcuckoo missing!")
  endif()
endif()
