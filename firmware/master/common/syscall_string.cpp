/* -*- mode: C; c-basic-offset: 4; intent-tabs-mode: nil -*-
 *
 * Thundercracker firmware
 *
 * Copyright <c> 2012 Sifteo, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Implementations for string operation syscalls.
 *
 * This is similar to the functionality in string.h, but provided
 * as a set of syscalls that operate on virtual memory.
 */

#include <sifteo/abi.h>
#include "svmmemory.h"
#include "svmruntime.h"
#include "crc.h"
#include "svmfastlz.h"

extern "C" {

#define MEMSET_BODY() {                                                 \
    if (SvmMemory::mapRAM(dest, mulsat16x16(sizeof *dest, count))) {    \
        while (count) {                                                 \
            *(dest++) = value;                                          \
            count--;                                                    \
        }                                                               \
    }                                                                   \
}

void _SYS_memset8(uint8_t *dest, uint8_t value, uint32_t count) MEMSET_BODY()
void _SYS_memset16(uint16_t *dest, uint16_t value, uint32_t count) MEMSET_BODY()
void _SYS_memset32(uint32_t *dest, uint32_t value, uint32_t count) MEMSET_BODY()

void _SYS_memcpy8(uint8_t *dest, const uint8_t *src, uint32_t count)
{
    FlashBlockRef ref;
    if (!SvmMemory::mapRAM(dest, count) ||
        !SvmMemory::copyROData(ref, dest, reinterpret_cast<SvmMemory::VirtAddr>(src), count))
        SvmRuntime::fault(F_SYSCALL_ADDRESS);
}

void _SYS_memcpy16(uint16_t *dest, const uint16_t *src, uint32_t count)
{
    // Currently implemented in terms of memcpy8. We may provide a
    // separate optimized implementation of this syscall in the future.   

    if (!isAligned(dest, 2) || !isAligned(src, 2))
        return SvmRuntime::fault(F_SYSCALL_ADDR_ALIGN);

    _SYS_memcpy8((uint8_t*) dest, (const uint8_t*) src, mulsat16x16(sizeof *dest, count));
}

void _SYS_memcpy32(uint32_t *dest, const uint32_t *src, uint32_t count)
{
    // Currently implemented in terms of memcpy8. We may provide a
    // separate optimized implementation of this syscall in the future.

    if (!isAligned(dest) || !isAligned(src))
        return SvmRuntime::fault(F_SYSCALL_ADDR_ALIGN);

    _SYS_memcpy8((uint8_t*) dest, (const uint8_t*) src, mulsat16x16(sizeof *dest, count));
}

uint32_t _SYS_crc32(const uint8_t *data, uint32_t count)
{
    SvmMemory::VirtAddr va = reinterpret_cast<SvmMemory::VirtAddr>(data);
    FlashBlockRef ref;
    uint32_t crc;

    if (!SvmMemory::crcROData(ref, va, count, crc)) {
        SvmRuntime::fault(F_SYSCALL_ADDRESS);
        return 0;
    }

    return crc;
}

uint32_t _SYS_decompress_fastlz1(uint8_t *dest, uint32_t destMax, const uint8_t *src, uint32_t srcLen)
{
    SvmMemory::VirtAddr srcVA = reinterpret_cast<SvmMemory::VirtAddr>(src);
    FlashBlockRef ref;

    if (!SvmMemory::mapRAM(dest, destMax) ||
        !SvmFastLZ::decompressL1(ref, dest, destMax, srcVA, srcLen)) {
        SvmRuntime::fault(F_SYSCALL_ADDRESS);
        return 0;
    }

    // Actual length, written by decompressL1()
    return destMax;
}

int32_t _SYS_memcmp8(const uint8_t *a, const uint8_t *b, uint32_t count)
{
    FlashBlockRef refA, refB;
    SvmMemory::VirtAddr vaA = reinterpret_cast<SvmMemory::VirtAddr>(a);
    SvmMemory::VirtAddr vaB = reinterpret_cast<SvmMemory::VirtAddr>(b);
    SvmMemory::PhysAddr paA, paB;

    while (count) {
        uint32_t chunkA = count;
        uint32_t chunkB = count;

        if (!SvmMemory::mapROData(refA, vaA, chunkA, paA)) {
            SvmRuntime::fault(F_SYSCALL_ADDRESS);
            break;
        }
        if (!SvmMemory::mapROData(refB, vaB, chunkB, paB)) {
            SvmRuntime::fault(F_SYSCALL_ADDRESS);
            break;
        }

        uint32_t chunk = MIN(chunkA, chunkB);
        vaA += chunk;
        vaB += chunk;
        count -= chunk;

        while (chunk) {
            int diff = *(paA++) - *(paB++);
            if (diff)
                return diff;
            chunk--;
        }
    }

    return 0;
}

uint32_t _SYS_strnlen(const char *str, uint32_t maxLen)
{
    FlashBlockRef ref;
    SvmMemory::VirtAddr va = reinterpret_cast<SvmMemory::VirtAddr>(str);
    SvmMemory::PhysAddr pa;
    
    uint32_t len = 0;

    while (len < maxLen) {
        uint32_t chunk = maxLen - len;
        if (!SvmMemory::mapROData(ref, va, chunk, pa)) {
            SvmRuntime::fault(F_SYSCALL_ADDRESS);
            break;
        }

        va += chunk;        
        while (chunk) {
            if (len == maxLen || !*pa)
                return len;
            pa++;
            len++;
            chunk--;
        }
    }
    
    return len;
}

void _SYS_strlcpy(char *dest, const char *src, uint32_t destSize)
{
    /*
     * Like the BSD strlcpy(), guaranteed to NUL-terminate.
     * We check the src pointer as we go, since the size is not known ahead of time.
     */
     
    if (destSize == 0)
        return;
    
    if (!SvmMemory::mapRAM(dest, destSize)) {
        SvmRuntime::fault(F_SYSCALL_ADDRESS);
        return;
    }

    FlashBlockRef ref;
    SvmMemory::VirtAddr srcVA = reinterpret_cast<SvmMemory::VirtAddr>(src);

    if (!SvmMemory::strlcpyROData(ref, dest, srcVA, destSize))
        SvmRuntime::fault(F_SYSCALL_ADDRESS);
}

void _SYS_strlcat(char *dest, const char *src, uint32_t destSize)
{
    if (destSize == 0)
        return;
    
    if (!SvmMemory::mapRAM(dest, destSize)) {
        SvmRuntime::fault(F_SYSCALL_ADDRESS);
        return;
    }

    FlashBlockRef ref;
    SvmMemory::VirtAddr srcVA = reinterpret_cast<SvmMemory::VirtAddr>(src);
    char *last = dest + destSize - 1;

    // Skip to end
    while (dest < last && *dest)
        dest++;

    // Append all the bytes we can
    while (dest < last) {
        uint8_t *p = SvmMemory::peek<uint8_t>(ref, srcVA);
        if (!p) {
            SvmRuntime::fault(F_SYSCALL_ADDRESS);
            break;
        }

        char c = *p;
        if (c) {
            *(dest++) = c;
            srcVA++;
        } else {
            break;
        }
    }

    // Guaranteed to NUL-termiante
    *dest = '\0';
}

void _SYS_strlcat_int(char *dest, int src, uint32_t destSize)
{
    if (destSize == 0)
        return;
    
    if (!SvmMemory::mapRAM(dest, destSize)) {
        SvmRuntime::fault(F_SYSCALL_ADDRESS);
        return;
    }

    char *last = dest + destSize - 1;

    // Skip to end
    while (dest < last && *dest)
        dest++;

    // Leading sign
    if (dest < last && src < 0) {
        *(dest++) = '-';
        src = -src;
    }

    // Calculate a floating divisor for the leftmost digit
    unsigned divisor = 1;
    for (;;) {
        unsigned nextDivisor = divisor * 10;
        if (nextDivisor > (unsigned)src)
            break;
        divisor = nextDivisor;
    }

    // Output all normal digits
    while (dest < last && divisor) {
        *(dest++) = '0' + (src / divisor) % 10;
        divisor /= 10;
    }

    // Guaranteed to NUL-termiante
    *dest = '\0';
}

void _SYS_strlcat_int_fixed(char *dest, int src, unsigned width, unsigned lz, uint32_t destSize)
{
    if (destSize == 0)
        return;
    
    if (!SvmMemory::mapRAM(dest, destSize)) {
        SvmRuntime::fault(F_SYSCALL_ADDRESS);
        return;
    }

    char *last = dest + destSize - 1;

    // Skip to end
    while (dest < last && *dest)
        dest++;

    // Write right-to-left. Position at the end, in an overflow-safe way.
    char *first = dest;
    width = MIN(width, unsigned(last - first));
    dest += width;
    ASSERT(dest >= first && dest <= last);

    // Start with the NUL terminator.
    *(dest--) = '\0';

    // Save the sign for later
    bool negative = src < 0;
    if (negative)
        src = -src;

    // First digit is always present
    if (dest >= first) {
        *(dest--) = '0' + (src % 10);
        src /= 10;
    }

    // Other digits, only if nonzero
    while (dest >= first && src) {
        *(dest--) = '0' + (src % 10);
        src /= 10;
    }

    // Leading characters
    while (dest >= first) {
        if (negative && (dest == first || !lz)) {
            *dest = '-';
            negative = false;
        } else if (lz) {
            *dest = '0';
        } else {
            *dest = ' ';
        }
        dest--;
    }
}

void _SYS_strlcat_int_hex(char *dest, int src, unsigned width, unsigned lz, uint32_t destSize)
{
    if (destSize == 0)
        return;
    
    if (!SvmMemory::mapRAM(dest, destSize)) {
        SvmRuntime::fault(F_SYSCALL_ADDRESS);
        return;
    }

    char *last = dest + destSize - 1;

    // Skip to end
    while (dest < last && *dest)
        dest++;

    // Write right-to-left. Position at the end, in an overflow-safe way.
    char *first = dest;
    width = MIN(width, unsigned(last - first));
    dest += width;
    ASSERT(dest >= first && dest <= last);

    // Start with the NUL terminator.
    *(dest--) = '\0';

    // Work backwards, writing each digit
    while (dest >= first) {
        static const char lut[] = "0123456789abcdef";
        *(dest--) = lut[src & 0xf];
        src >>= 4;
    }
}

int32_t _SYS_strncmp(const char *a, const char *b, uint32_t count)
{
    FlashBlockRef refA, refB;
    SvmMemory::VirtAddr vaA = reinterpret_cast<SvmMemory::VirtAddr>(a);
    SvmMemory::VirtAddr vaB = reinterpret_cast<SvmMemory::VirtAddr>(b);
    SvmMemory::PhysAddr paA, paB;

    while (count) {
        uint32_t chunkA = count;
        uint32_t chunkB = count;

        if (!SvmMemory::mapROData(refA, vaA, chunkA, paA)) {
            SvmRuntime::fault(F_SYSCALL_ADDRESS);
            break;
        }
        if (!SvmMemory::mapROData(refB, vaB, chunkB, paB)) {
            SvmRuntime::fault(F_SYSCALL_ADDRESS);
            break;
        }

        uint32_t chunk = MIN(chunkA, chunkB);
        vaA += chunk;
        vaB += chunk;
        count -= chunk;

        while (chunk) {
            uint8_t aV = *(paA++);
            uint8_t bV = *(paB++);
            int diff = aV - bV;
            if (diff)
                return diff;
            if (!aV)
                return 0;
            chunk--;
        }
    }

    return 0;
}

}  // extern "C"
