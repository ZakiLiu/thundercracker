/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * This file is part of the internal implementation of the Sifteo SDK.
 * Confidential, not for redistribution.
 *
 * Copyright <c> 2011 Sifteo, Inc. All rights reserved.
 */

#include "radio.h"

namespace Sifteo {

void Sifteo::RadioManager::produce(PacketTransmission &tx)
{
    static RadioAddress addr = { 0x02, { 0xe7, 0xe7, 0xe7, 0xe7, 0xe7 }};

    tx.dest = &addr;
    tx.packet.len = 5;
}

void Sifteo::RadioManager::acknowledge(const PacketBuffer &packet)
{
}

void Sifteo::RadioManager::timeout()
{
}

};  // namespace Sifteo
