/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "LAppAllocator.hpp"

#include <cstdlib>
#include <cstddef>

using namespace Csm;

void* LAppAllocator::Allocate(const csmSizeType size)
{
    return malloc(size);
}

void LAppAllocator::Deallocate(void* memory)
{
    if (memory)
    {
        free(memory);
    }
}

void* LAppAllocator::AllocateAligned(const csmSizeType size, const csmUint32 alignment)
{
    // Manual aligned allocation:
    // Allocate extra space for alignment padding and a preamble pointer.
    const size_t offset = alignment - 1 + sizeof(void*);
    void* allocation = Allocate(size + static_cast<csmUint32>(offset));
    if (!allocation)
    {
        return nullptr;
    }

    // Compute the aligned address.
    size_t alignedAddress = reinterpret_cast<size_t>(allocation) + sizeof(void*);
    const size_t shift = alignedAddress % alignment;
    if (shift)
    {
        alignedAddress += (alignment - shift);
    }

    // Store the original allocation pointer just before the aligned address
    // so we can recover it during deallocation.
    void** preamble = reinterpret_cast<void**>(alignedAddress);
    preamble[-1] = allocation;

    return reinterpret_cast<void*>(alignedAddress);
}

void LAppAllocator::DeallocateAligned(void* alignedMemory)
{
    if (!alignedMemory)
    {
        return;
    }

    // Recover the original allocation pointer stored before the aligned address.
    void** preamble = static_cast<void**>(alignedMemory);
    Deallocate(preamble[-1]);
}
