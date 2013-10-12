// ============================================================================
//  MemoryRequest.cpp: Memory request object.
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
//  along with MARSS.  If not, see <http://www.gnu.org/licenses/>.
//
//  Copyright 2013 Tyler Stachecki <tstache1@cs.binghamton.edu>
//  Copyright 2009 Avadh Patel <apatel@cs.binghamton.edu>
//  Copyright 2009 Furat Afram <fafram@cs.binghamton.edu>
// ============================================================================
#ifndef __MEMORYREQUEST_H__
#define __MEMORYREQUEST_H__
#include <cstdint>
#include "MemoryOperations.h"
#include "PoolAllocator.h"

// TODO: Remove after types are changed.
#include <globals.h>

namespace Memory {

class MemoryRequest {
  static PoolAllocator<MemoryRequest, 1024> Allocator;

  Signal *coreSignal;
  stringbuf* history;

  uint64_t physicalAddress;
  uint64_t cycles;

  uint64_t ownerRIP;
  uint64_t ownerUUID;
  unsigned coreId;
  unsigned threadId;
  int refCounter;
  int robId;

  OperationType operationType;
  bool isData;

public:
  //
  // Constructors just reset/set/copy all fields accordingly.
  //
  MemoryRequest() : coreSignal(NULL), history(NULL), physicalAddress(0),
    cycles(0), ownerRIP(0), ownerUUID(0), coreId(0), threadId(0), refCounter(0),
    robId(0), operationType(OPERATION_INVALID), isData(0) {}

  MemoryRequest(unsigned coreId_, unsigned threadId_, uint64_t physicalAddress_,
    int robId_, uint64_t cycles_, bool isInstruction, uint64_t ownerRIP_, 
    uint64_t ownerUUID_, OperationType operationType_) :

    coreSignal(NULL), history(NULL), physicalAddress(physicalAddress_),
    cycles(cycles_), ownerRIP(ownerRIP_), ownerUUID(ownerUUID_),
    coreId(coreId_), threadId(threadId_), refCounter(0), robId(robId_),
    operationType(operationType_), isData(!isInstruction) {

	  if(history)
      delete history;

	  history = new stringbuf();
	  //memdebug("Init ", *this, endl);
  }

  MemoryRequest(const MemoryRequest& r) :
    coreSignal(NULL), history(NULL), physicalAddress(r.physicalAddress),
    cycles(r.cycles), ownerRIP(r.ownerRIP), ownerUUID(r.ownerUUID),
    coreId(r.coreId), threadId(r.threadId), refCounter(0), robId(r.robId),
    operationType(r.operationType), isData(r.isData) {

	  if(history)
      delete history;

	  history = new stringbuf();
	  //memdebug("Init ", *this, endl);
  }

  //
  // Quickly allocate/deallocate a MemoryRequest.
  //
  void operator delete(void* pointer, size_t count) {
    Allocator.deallocate((MemoryRequest*) pointer);
  }

  void* operator new(size_t count) {
    assert(count == sizeof(MemoryRequest));
    return Allocator.allocate(1);
  }

  //
  // Overloaded comparison operator.
  //
  bool operator==(const MemoryRequest& request) {
    return
      this->coreId == request.coreId &&
      this->threadId == request.threadId &&
      this->robId == request.robId &&
      this->physicalAddress == request.physicalAddress &&
      this->operationType == request.operationType &&
      this->isData == request.isData;
  }

  //
  // Overloaded ostream operator for output.
  //
  friend ostream& operator <<(ostream& os, const MemoryRequest& request);

  //
  // Decrements/increments the reference counter.
  //
  void decRefCounter() {
    refCounter--;
  }

  void incRefCounter() {
    refCounter++;
  }

  //
  // Gets the memory operation type (enum).
  //
  OperationType getType() const {
    return operationType;
  }

  const char* getTypeLiteral() const {
    return OperationNames[operationType];
  }

  //
  // Sets the memory operation type.
  //
  void setType(OperationType type) {
    assert(operationType < NUM_OPERATION_TYPES);
    assert(operationType > 0);
    operationType = type;
  }

    int get_ref_counter() {
      return refCounter;
    }

    void set_ref_counter(int count) {
      refCounter = count;
    }

    bool is_instruction() {
      return !isData;
    }

    W64 get_physical_address() { return physicalAddress; }
    void set_physical_address(W64 addr) { physicalAddress = addr; }

    int get_coreid() { return int(coreId); }

    int get_threadid() { return int(threadId); }

    int get_robid() { return robId; }
    void set_robid(int idx) { robId = idx; }

    W64 get_owner_rip() { return ownerRIP; }

    W64 get_owner_uuid() { return ownerUUID; }

    W64 get_init_cycles() { return cycles; }

    stringbuf& get_history() { return *history; }

        bool is_kernel() {
            // based on owner RIP value
            if(bits(ownerRIP, 48, 16) != 0) {
                return true;
            }
            return false;
        }

        void set_coreSignal(Signal* signal)
        {
            coreSignal = signal;
        }

        Signal* get_coreSignal()
        {
            return coreSignal;
        }

};

inline ostream& operator <<(ostream& os, const MemoryRequest& request) {
  os << "Memory Request: core[", request.coreId, "] ";
  os << "thread[", request.threadId, "] ";
  os << "address[0x", hexstring(request.physicalAddress, 48), "] ";
  os << "robid[", request.robId, "] ";
  os << "init-cycle[", request.cycles, "] ";
  os << "ref-counter[", request.refCounter, "] ";
  os << "op-type[", request.getTypeLiteral(), "] ";
  os << "isData[", request.isData, "] ";
  os << "ownerUUID[", request.ownerUUID, "] ";
  os << "ownerRIP[", (void*) (request.ownerRIP), "] ";
  os << "History[ " << *(request.history) << "] ";

  if(request.coreSignal)
    os << "Signal[ " << request.coreSignal->get_name() << "] ";

  return os;
}

}

#endif

