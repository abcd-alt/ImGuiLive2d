/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#pragma once

#include <CubismFramework.hpp>
#include <ICubismAllocator.hpp>

/**
 * @brief  Memory allocator implementation for the Cubism Framework.
 *
 * Implements the ICubismAllocator interface using standard malloc/free.
 * This is a standalone version that does not depend on the Common sample files.
 */
class LAppAllocator : public Csm::ICubismAllocator
{
public:
    /**
     * @brief  Allocate a block of memory.
     *
     * @param[in]  size  Desired size in bytes.
     *
     * @return  Pointer to the allocated memory, or nullptr on failure.
     */
    virtual void* Allocate(const Csm::csmSizeType size);

    /**
     * @brief  Deallocate a previously allocated block of memory.
     *
     * @param[in]  memory  Pointer to the memory to free.
     */
    virtual void Deallocate(void* memory);

    /**
     * @brief  Allocate a block of memory with the specified alignment.
     *
     * @param[in]  size       Desired size in bytes.
     * @param[in]  alignment  Desired alignment in bytes.
     *
     * @return  Pointer to the aligned memory, or nullptr on failure.
     */
    virtual void* AllocateAligned(const Csm::csmSizeType size, const Csm::csmUint32 alignment);

    /**
     * @brief  Deallocate a previously aligned block of memory.
     *
     * @param[in]  alignedMemory  Pointer to the aligned memory to free.
     */
    virtual void DeallocateAligned(void* alignedMemory);
};
