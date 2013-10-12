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
#include "MemoryRequest.h"
#include "PoolAllocator.h"
#include <iostream>

namespace Memory {

// Request pool allocator; quickly allocate/move/release requests.
PoolAllocator <MemoryRequest, 1024> MemoryRequest::Allocator;

//
// Overloaded ostream operator for output.
//
std::ostream& operator<<(std::ostream& os, const MemoryRequest& request) {
  os << "Memory Request: core[" << request.getCoreId() << "] ";
  os << "thread[" << request.getThreadId() << "] ";
  os << "address[0x" << hexstring(request.getPhysicalAddress(), 48) << "] ";
  os << "robid[" << request.getRobId() << "] ";
  os << "init-cycle[" << request.getInitCycles() << "] ";
  os << "ref-counter[" << request.getRefCounter() << "] ";
  os << "op-type[" << request.getTypeLiteral() << "] ";
  os << "isData[" << !request.isInstruction() << "] ";
  os << "ownerUUID[" << request.getOwnerUUID() << "] ";
  os << "ownerRIP[" << (void*) (request.getOwnerRIP()) << "] ";
  os << "History[ " << *(request.get_history()) << "] ";

  if(request.get_coreSignal())
    os << "Signal[ " << request.get_coreSignal()->get_name() << "] ";

  return os;
}

}

