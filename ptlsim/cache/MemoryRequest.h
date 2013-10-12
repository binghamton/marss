// ============================================================================
//  MemoryOps.h: Memory operation types.
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
#ifndef __MEMORYREQUEST_H__
#define __MEMORYREQUEST_H__
#include <globals.h>
#include <superstl.h>
#include <statelist.h>
#include <cacheConstants.h>


#include <cstdint>

#include "MemoryOperations.h"
#include "PoolAllocator.h"

namespace Memory {

class MemoryRequest: public selfqueuelink {
  static PoolAllocator<MemoryRequest, 1024> Allocator;
  OperationType operationType;

  uint64_t physicalAddress_;

  uint64_t cycles_;
  uint64_t ownerRIP_;
  uint64_t ownerUUID_;

  unsigned coreId_;
  unsigned threadId_;
  bool isData_;
  int robId_;
  int refCounter_;
  stringbuf *history;
  Signal *coreSignal_;

public:
  MemoryRequest() {
    reset();
  }

  // Quickly deallocate a MemoryRequest.
  void operator delete(void* pointer, size_t count) {
    Allocator.deallocate((MemoryRequest*) pointer);
  }

  // Quickly allocate a MemoryRequest.
  void* operator new(size_t count) {
    assert(count == sizeof(MemoryRequest));
    return Allocator.allocate(1);
  }

  // Gets the memory operation type (enum).
  OperationType getType() const {
    return operationType;
  }

  // Gets the memory operation type (literal).
  const char* getTypeLiteral() const {
    return OperationNames[operationType];
  }

  // Sets the memory operation type.
  void setType(OperationType type) {
    assert(operationType < NUM_OPERATION_TYPES);
    assert(operationType > 0);
    operationType = type;
  }

    void reset() {
      operationType = OPERATION_INVALID;

      coreId_ = 0;
      threadId_ = 0;
      physicalAddress_ = 0;
      robId_ = 0;
      cycles_ = 0;
      ownerRIP_ = 0;
      refCounter_ = 0; // or maybe 1
      isData_ = 0;
      history = new stringbuf();
            coreSignal_ = NULL;
    }

    void incRefCounter(){
      refCounter_++;
    }

    void decRefCounter(){
      refCounter_--;
    }

    void init(W8 coreId,
        W8 threadId,
        W64 physicalAddress,
        int robId,
        W64 cycles,
        bool isInstruction,
        W64 ownerRIP,
        W64 ownerUUID,
        OperationType opType);

    bool is_same(W8 coreid,
        W8 threadid,
        int robid,
        W64 physaddr,
        bool is_icache,
        bool is_write);
    bool is_same(MemoryRequest *request);

    void init(MemoryRequest *request);

    int get_ref_counter() {
      return refCounter_;
    }

    void set_ref_counter(int count) {
      refCounter_ = count;
    }

    bool is_instruction() {
      return !isData_;
    }

    W64 get_physical_address() { return physicalAddress_; }
    void set_physical_address(W64 addr) { physicalAddress_ = addr; }

    int get_coreid() { return int(coreId_); }

    int get_threadid() { return int(threadId_); }

    int get_robid() { return robId_; }
    void set_robid(int idx) { robId_ = idx; }

    W64 get_owner_rip() { return ownerRIP_; }

    W64 get_owner_uuid() { return ownerUUID_; }

    W64 get_init_cycles() { return cycles_; }

    stringbuf& get_history() { return *history; }

        bool is_kernel() {
            // based on owner RIP value
            if(bits(ownerRIP_, 48, 16) != 0) {
                return true;
            }
            return false;
        }

        void set_coreSignal(Signal* signal)
        {
            coreSignal_ = signal;
        }

        Signal* get_coreSignal()
        {
            return coreSignal_;
        }

    ostream& print(ostream& os) const
    {
      os << "Memory Request: core[", coreId_, "] ";
      os << "thread[", threadId_, "] ";
      os << "address[0x", hexstring(physicalAddress_, 48), "] ";
      os << "robid[", robId_, "] ";
      os << "init-cycle[", cycles_, "] ";
      os << "ref-counter[", refCounter_, "] ";
      os << "op-type[", getTypeLiteral(), "] ";
      os << "isData[", isData_, "] ";
      os << "ownerUUID[", ownerUUID_, "] ";
      os << "ownerRIP[", (void*)ownerRIP_, "] ";
      os << "History[ " << *history << "] ";
            if(coreSignal_) {
                os << "Signal[ " << coreSignal_->get_name() << "] ";
            }
      return os;
    }
};

static inline ostream& operator <<(ostream& os, const MemoryRequest& request)
{
  return request.print(os);
}

};

#endif //MEMORY_REQUEST_H
