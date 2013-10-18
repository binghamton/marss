// ============================================================================
//  MemoryController.h: Memory controller class.
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
#ifndef __MEMORYCONTROLLER_H__
#define __MEMORYCONTROLLER_H__
#include "controller.h"
#include "interconnect.h"
#include "MemoryRequest.h"
#include "memoryStats.h"
#include <deque>

namespace Memory {

struct MemoryQueueEntry {
  //
  // TODO/FIXME: These shouldn't be public...
  //
  MemoryRequest *request;
  Controller *source;
  bool annuled;
  bool inUse;

public:
  //
  // Default constructor just resets all class members.
  //
  MemoryQueueEntry() : request(NULL),
    annuled(false), inUse(false) {}

  //
  // Overloaded ostream operator for output.
  //
  friend std::ostream& operator<<(std::ostream& os,
    const MemoryQueueEntry& entry);
};

class MemoryController : public Controller {
  std::deque<MemoryQueueEntry*> pendingRequests;
  Interconnect *cacheInterconnect;

  Signal accessCompleted;
  Signal waitInterconnect;

  int latency;
  int bankBits;
  uint8_t banksUsed[MEM_BANKS];

  RAMStats new_stats;

  //
  // Get bank ID from address using cache line interleaving
  // address mapping (using lower bits for the bank ID).
  //
  unsigned getBankID(uint64_t address) const {
    return lowbits(address >> 6, bankBits);
  }

public:
  MemoryController(W8 coreid, const char *name,
    MemoryHierarchy *memoryHierarchy);
  virtual bool handle_interconnect_cb(void *arg);
  void print(ostream& os) const;

  virtual void register_interconnect(Interconnect *interconnect, int type);

  virtual bool access_completed_cb(void *arg);
  virtual bool wait_interconnect_cb(void *arg);

  void annul_request(MemoryRequest *request);
  virtual void dump_configuration(YAML::Emitter &out) const;

  virtual int get_no_pending_request(W8 coreid);

  bool is_full(bool fromInterconnect = false) const {
    return pendingRequests.size() > MEM_REQ_NUM;
  }

  void print_map(ostream& os) {
    os << "Memory Controller: " << get_name() << endl;
    os << "\tconnected to:" << endl;
    os << "\t\tinterconnect: " << cacheInterconnect->get_name() << endl;
  }
};

}

#endif

