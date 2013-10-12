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
#include <cassert>
#include <cstdint>
#include <iostream>
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
  // Constructors just set/copy all fields accordingly.
  //
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
  friend std::ostream& operator<<(std::ostream& os,
    const MemoryRequest& request);

  //
  // Decrements/increments the reference counter.
  //
  void decRefCounter() {
    assert(refCounter > 0);
    refCounter--;
  }

  void incRefCounter() {
    refCounter++;
  }

  //
  // Class getters.
  //
  unsigned getCoreId() const { return coreId; }
  uint64_t getInitCycles() const { return cycles; }
  uint64_t getOwnerRIP() const { return ownerRIP; }
  uint64_t getOwnerUUID() const { return ownerUUID; }
  uint64_t getPhysicalAddress() const { return physicalAddress; }
  int getRefCounter() const { return refCounter; }
  int getRobId() const { return robId; }
  unsigned getThreadId() const { return threadId; }
  OperationType getType() const { return operationType; }
  const char* getTypeLiteral() const { return OperationNames[operationType]; }
  bool isInstruction() const { return !isData; }

  //
  // Class setters.
  //
  void setType(OperationType type) {
    assert(operationType < NUM_OPERATION_TYPES);
    assert(operationType > 0);
    operationType = type;
  }

  void setPhysicalAddress(uint64_t addr) { this->physicalAddress = addr; }
  void setRefCounter(int refCounter) { this->refCounter = refCounter; }
  void setRobId(int robId) { this->robId = robId; }

  //
  // TODO: Finish removing superstl dependencies.
  //
  stringbuf& get_history() const { return *history; }
  Signal* get_coreSignal() const { return coreSignal; }
  void set_coreSignal(Signal* coreSignal) { this->coreSignal = coreSignal; }

  bool is_kernel() { return bits(ownerRIP, 48, 16) != 0; }
};

}

#endif

