// ============================================================================
//  PoolAllocator.h: Generic, fast slab/pool allocator.
//
//  MARSS is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  MARSS is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
//
//  Copyright 2013 Tyler Stachecki <tstache1@cs.binghamton.edu>
//  Copyright 2009 Avadh Patel <apatel@cs.binghamton.edu>
//  Copyright 2009 Furat Afram <fafram@cs.binghamton.edu>
// ============================================================================
#ifndef __POOLALLOCATOR_H__
#define __POOLALLOCATOR_H__
#include <cassert>
#include <cstddef>
#include <cstring>
#include <memory>
#include <new>

#ifdef __GNUC__
#define unused(var) UNUSED__##var __attribute__((unused))
#else
#define unused(var)
#endif

// TODO: Remove.
#include <globals.h>
#include <superstl.h>

//
// This is a pool allocator that does NOT cache color (but could
// easily be extended to do so). It also doesn't meet all requirements
// of a std::allocator, but, again, could be extended at a later
// time to do so if needed.
//

//
// Also note that this pool/freelist never contracts; it only expands.
// If your workloads are highly variable in size, then fragmentation
// and pool bloat might become an issue for you.
//

template <class T, unsigned BlocksPerPool>
struct PoolAllocator {
  typedef T               value_type;
  typedef T*              pointer;
  typedef const T*        const_pointer;
  typedef T&              reference;
  typedef const T&        const_reference;
  typedef std::size_t     size_type;
  typedef std::ptrdiff_t  difference_type;

private:
  char initialBlocks[sizeof(T) * BlocksPerPool];

  struct PoolBlock {
    char block[sizeof(T) * BlocksPerPool];
    PoolBlock* next;
  };

  // Free list stuff...
  size_t freelistSize;
  size_t freelistHead;
  pointer* freelist;
  PoolBlock *blocks;

  // Expands the size of the free list.
  void expandFreelist() {
    pointer* newfreelist = new pointer[freelistSize * 2];
    memcpy(newfreelist, freelist, sizeof(pointer) * freelistSize);

    delete[] freelist;
    freelist = newfreelist;
    freelistSize *= 2;
  }

  // Expands the pool by adding a new block.
  // Assumes freelist is empty, fills it up.
  void expandPool() {
    unsigned i;

    // Allocate a new block.
    PoolBlock* block = new PoolBlock();
    block->next = blocks;
    blocks = block;

    // Fill the free list.
    freelistHead = BlocksPerPool;
    for (i = 0; i < BlocksPerPool; i++)
      freelist[i] = (pointer) &block->block[i * sizeof(T)];
  }

public:
  // Allocate some memory.
  PoolAllocator() : blocks(NULL) {
    unsigned i;

    freelistHead = freelistSize = BlocksPerPool;
    freelist = new pointer[BlocksPerPool];

    for (i = 0; i < BlocksPerPool; i++)
      freelist[i] = (pointer) &initialBlocks[i * sizeof(T)];
  }

  // Destroy free list, heap-allocated blocks.
  ~PoolAllocator() {
    PoolBlock *block = blocks;
    delete[] freelist;

    while (block != NULL) {
      PoolBlock *nextBlock = block->next;
      delete block;

      block = nextBlock;
    }
  }

  // Allocate a piece of the pool and return it.
  pointer allocate(size_type n = 1, std::allocator<void>::
    const_pointer unused(hint) = 0) {
    assert(n == 1);

    // Don't support allocation of arrays.
    if (unlikely(n != 1)) throw std::bad_alloc();

    if (unlikely(freelistHead == 0))
      expandPool();

    return freelist[--freelistHead];
  }

  // Take some of the user's memory and reclaim it.
  void deallocate(pointer p, size_type n = 1) {
    assert(n == 1);

    // Don't support deallocation of arrays.
    if (unlikely(n != 1)) throw std::bad_alloc();

    if (unlikely(freelistHead >= freelistSize))
      expandFreelist();

    freelist[freelistHead++] = p; 
  }

  // Returns the maximum theoretical possible value of n.
  // for which the call allocate(n, 0) could succeed.
  size_type max_size() const {
    return 1;
  }

  // Construct allocated object in-place.
  template<class U, class... Args>
  void construct(U* p, Args&&... args) {
    ::new((void *)p) U(std::forward<Args>(args)...);
  }

  // Destruct an object pointed to by p.
  template<class U>
  void destroy(U* p) {
    p->~U();
  }
};

#endif

