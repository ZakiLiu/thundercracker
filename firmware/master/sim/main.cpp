/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * This file is part of the internal implementation of the Sifteo SDK.
 * Confidential, not for redistribution.
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

/*
 * Entry point program for simulation use, i.e. when compiling for a
 * desktop OS rather than for the actual master cube.
 */

#include <sifteo/abi.h>
#include "radio.h"
#include "runtime.h"
#include "systime.h"


int main(int argc, char **argv)
{
    SysTime::init();
    Radio::open();

    Runtime::run();

    return 0;
}
