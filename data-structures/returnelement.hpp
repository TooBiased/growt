/*******************************************************************************
 * data-structures/returnelement.h
 *
 * Some definitions that are important as function return values.
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include <stdlib.h>
#include <tuple>

namespace growt
{

enum class ReturnCode
{
    // bit pattern:  1 = success ;   2 = not found key ;   4 = found key
    //               8 = insert  ;  16 = update        ;  32 = delete
    //              64 = full    ; 128 = invalid cell  ; 256 = backoff

    ERROR = 0,

    SUCCESS_IN  = 9,  // success+insert
    SUCCESS_UP  = 17, // success+update
    SUCCESS_DEL = 33, // success+delete

    UNSUCCESS_NOT_FOUND    = 2,
    UNSUCCESS_ALREADY_USED = 4,
    UNSUCCESS_FULL         = 64,
    UNSUCCESS_INVALID      = 128,
    UNSUCCESS_BACKOFF      = 256
};

inline bool successful(ReturnCode ec) { return (static_cast<uint>(ec) & 1u); }

} // namespace growt
