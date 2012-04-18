/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Sifteo Thundercracker simulator
 * M. Elizabeth Scott <beth@sifteo.com>
 *
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include <stdio.h>
#include <setjmp.h>
#include <errno.h>

#include "system.h"
#include "system_mc.h"
#include "macros.h"
#include "radio.h"
#include "systime.h"
#include "audiooutdevice.h"
#include "audiomixer.h"
#include "flash.h"
#include "flashlayer.h"
#include "assetmanager.h"
#include "svmloader.h"
#include "svmcpu.h"
#include "svmruntime.h"
#include "mc_gdbserver.h"

SystemMC *SystemMC::instance;


bool SystemMC::installELF(const char *path)
{
    FILE *elfFile = fopen(path, "rb");
    if (elfFile == NULL) {
        LOG(("Error, couldn't open ELF file '%s' (%s)\n", path, strerror(errno)));
        return false;
    }

    // write the file to external flash
    uint8_t buf[512];
    Flash::chipErase();

    unsigned addr = 0;
    while (!feof(elfFile)) {
        unsigned rxed = fread(buf, 1, sizeof(buf), elfFile);
        if (rxed > 0) {
            Flash::write(addr, buf, rxed);
            addr += rxed;
        }
    }
    fclose(elfFile);
    Flash::flush();

    return true;
}

bool SystemMC::init(System *sys)
{
    this->sys = sys;
    instance = 0;

    Flash::init();
    FlashBlock::init();
    AssetManager::init();

    if (sys->opt_svmTrace)
        SvmCpu::enableTracing();
    if (sys->opt_svmFlashStats)
        FlashBlock::enableStats();
    if (sys->opt_svmStackMonitor)
        SvmRuntime::enableStackMonitoring();

    if (!sys->opt_elfFile.empty() && !installELF(sys->opt_elfFile.c_str()))
        return false;

    return true;
}

void SystemMC::start()
{
    mThreadRunning = true;
    instance = this;
    __asm__ __volatile__ ("" : : : "memory");
    mThread = new tthread::thread(threadFn, 0);
}

void SystemMC::stop()
{
    mThreadRunning = false;
    __asm__ __volatile__ ("" : : : "memory");
    mThread->join();
    delete mThread;
    mThread = 0;
}

void SystemMC::exit()
{
    // Nothing to do yet
}

void SystemMC::threadFn(void *param)
{
    if (setjmp(instance->mThreadExitJmp))
        return;

    // Start the master at some point shortly after the cubes come up
    instance->ticks = instance->sys->time.clocks + STARTUP_DELAY;

    AudioOutDevice::init(AudioOutDevice::kHz16000, &AudioMixer::instance);
    AudioOutDevice::start();

    Radio::open();
    GDBServer::start(2345);

    SvmLoader::run(111);
}

SysTime::Ticks SysTime::ticks()
{
    /*
     * Our TICK_HZ divides easily into nanoseconds (62.5 ns at 16 MHz)
     * so we can do this conversion using fixed-point math quite easily.
     *
     * This does it in 64-bit math, with 60.4 fixed-point.
     */

    return ((SystemMC::instance->ticks * hzTicks(SystemMC::TICK_HZ / 16)) >> 4);
}

void Radio::open()
{
    // Nothing to do in simulation
}

void Radio::halt()
{
    SystemMC *smc = SystemMC::instance;

    // Are we trying to stop() the MC thread?
    if (!smc->mThreadRunning)
        longjmp(smc->mThreadExitJmp, 1);

    smc->doRadioPacket();
}

void SystemMC::doRadioPacket()
{
    // Prepare buffers
    struct {
        PacketTransmission ptx;
        PacketBuffer prx;
        Cube::Radio::Packet packet;
        Cube::Radio::Packet reply;
        bool ack;
        unsigned cubeID;
    } buf;
    memset(&buf, 0, sizeof buf);
    buf.ptx.packet.bytes = buf.packet.payload;
    buf.prx.bytes = buf.reply.payload;

    // MC firmware produces a packet
    RadioManager::produce(buf.ptx);
    ASSERT(buf.ptx.dest != NULL);
    uint64_t addr = buf.ptx.dest->pack();
    buf.packet.len = buf.ptx.packet.len;

    for (unsigned retry = 0; retry < MAX_RETRIES; ++retry) {

        beginPacket();

        // Deliver it to the proper cube
        for (unsigned i = 0; i < sys->opt_numCubes; i++) {
            Cube::Radio &radio = sys->cubes[i].spi.radio;

            if (radio.getPackedRXAddr() == addr &&
                radio.handlePacket(buf.packet, buf.reply)) {
                buf.ack = true;
                buf.cubeID = i;
                break;
            }
        }

        // Log this transaction
        if (sys->opt_radioTrace) {
            LOG(("RADIO: %6dms %02d/%02x%02x%02x%02x%02x -- TX[%2d] ",
                int(SysTime::ticks() / SysTime::msTicks(1)),
                buf.ptx.dest->channel,
                buf.ptx.dest->id[4],
                buf.ptx.dest->id[3],
                buf.ptx.dest->id[2],
                buf.ptx.dest->id[1],
                buf.ptx.dest->id[0],
                buf.packet.len));

            for (unsigned i = 0; i < sizeof buf.packet.payload; i++) {
                if (i < buf.packet.len)
                    LOG(("%02x", buf.packet.payload[i]));
                else
                    LOG(("  "));
            }

            if (buf.ack) {
                LOG((" -- Cube %d: ACK[%2d] ", buf.cubeID, buf.reply.len));
                for (unsigned i = 0; i < buf.reply.len; i++)
                    LOG(("%02x", buf.reply.payload[i]));
                LOG(("\n"));
            } else {
                LOG((" -- TIMEOUT, retry #%d\n", retry));
            }
        }

        // Send the response
        if (buf.ack) {
            if (buf.reply.len) {
                buf.prx.len = buf.reply.len;
                RadioManager::ackWithPacket(buf.prx);
            } else {
                RadioManager::ackEmpty();
            }
            endPacket();
            return;
        }
    }
    
    // Out of retries
    RadioManager::timeout();
    endPacket();
}

void SystemMC::beginPacket()
{
    // Advance time, and rally with the cube thread at the proper timestamp.
    // Between beginCubeEvent() and endCubeEvent(), both simulation threads are synchronized.
    ticks += SystemMC::TICKS_PER_PACKET;
    sys->beginCubeEvent(ticks);
    ASSERT(ticks == sys->time.clocks);
}

void SystemMC::endPacket()
{
    // Let the cube keep running, but no farther than our next transmit opportunity
    sys->endCubeEvent(ticks + SystemMC::TICKS_PER_PACKET);
}
