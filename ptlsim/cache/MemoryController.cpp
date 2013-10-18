// ============================================================================
//  MemoryController.cpp: Memory controller class.
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
#include "MemoryController.h"
#include <memoryHierarchy.h>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <deque>

#include <machine.h>

using namespace Memory;

//
// Overloaded ostream operator for output.
//
std::ostream& operator<<(std::ostream& os, const MemoryQueueEntry& entry) {
  if(entry.request)
    os << "Request{" << *entry.request << "} ";

  if (entry.source)
    os << "source[" << entry.source->get_name() <<  "] ";

  os << "annuled[" << entry.annuled << "] ";
  os << "inUse[" << entry.inUse << "]\n";

  return os;
}

MemoryController::MemoryController(W8 coreid, const char *name,
    MemoryHierarchy *memoryHierarchy) :
  Controller(coreid, name, memoryHierarchy)
    , new_stats(name, &memoryHierarchy->get_machine())
{
  memoryHierarchy_->add_cache_mem_controller(this);

  if(!memoryHierarchy->get_machine().get_option(name, "latency", latency))
      latency = 50;

  /* Convert latency from ns to cycles */
  latency = ns_to_simcycles(latency);

  SET_SIGNAL_CB(name, "_Access_Completed", accessCompleted,
    &MemoryController::access_completed_cb);

  SET_SIGNAL_CB(name, "_Wait_Interconnect", waitInterconnect,
    &MemoryController::wait_interconnect_cb);

  bankBits = log2(MEM_BANKS);
  memset(banksUsed, 0, sizeof(banksUsed));
}

void MemoryController::register_interconnect(
  Interconnect *interconnect, int type) {

  assert(type == INTERCONN_TYPE_UPPER &&
    "Can only register upper interconnects.");

  cacheInterconnect = interconnect;
}

bool MemoryController::handle_interconnect_cb(void *arg)
{
  Message *message = (Message*)arg;

  memdebug("Received message in Memory controller: ", *message, endl);

  if(message->hasData && message->request->getType() !=
      OPERATION_UPDATE)
    return true;

    if (message->request->getType() == OPERATION_EVICT) {
        /* We ignore all the evict messages */
        return true;
    }

  /*
   * if this request is a memory update request then
   * first check the pending queue and see if we have a
   * memory update request to same line and if we can merge
   * those requests then merge them into one request
   */
  if(message->request->getType() == OPERATION_UPDATE) {
    MemoryQueueEntry *entry;

    for(auto iter = pendingRequests.rbegin();
      iter != pendingRequests.rend(); ++iter) {
      entry = *iter;

      if(entry->request->getPhysicalAddress() ==
          message->request->getPhysicalAddress()) {
        /*
         * found an request for same line, now if this
         * request is memory update then merge else
         * don't merge to maintain the serialization
         * order
         */
        if(!entry->inUse && entry->request->getType() ==
            OPERATION_UPDATE) {
          /*
           * We can merge the request, so in simulation
           * we dont have data, so don't do anything
           */
          return true;
        }
        /*
         * we can't merge the request, so do normal
         * simuation by adding the entry to pending request
         * queue.
         */
        break;
      }
    }
  }

  //MemoryQueueEntry *queueEntry = pendingRequests_.alloc();

  /* if queue is full return false to indicate failure */
  if(pendingRequests.size() > (MEM_REQ_NUM)) {
    memdebug("Memory queue is full\n");
    return false;
  }

  if(pendingRequests.size() > (MEM_REQ_NUM - 1)) {
    memoryHierarchy_->set_controller_full(this, true);
  }

  MemoryQueueEntry* queueEntry = new MemoryQueueEntry();
  queueEntry->request = message->request;
  queueEntry->source = (Controller*) message->origin;
  queueEntry->request->incRefCounter();
  ADD_HISTORY_ADD(queueEntry->request);
  queueEntry->inUse = false;

  int bank_no = getBankID(message->request->
      getPhysicalAddress());

  assert(queueEntry->inUse == false);
  pendingRequests.push_back(queueEntry);

  if(banksUsed[bank_no] == 0) {
    banksUsed[bank_no] = 1;
    queueEntry->inUse = true;
    marss_add_event(&accessCompleted, latency,
        queueEntry);
  }

  return true;
}

void MemoryController::print(ostream& os) const {
  unsigned i;

  os << "---Memory-Controller: " << get_name() << std::endl;

  if(pendingRequests.size() > 0) {
    os << "Queue : ";

    for (auto iter : pendingRequests)
      std::cout << iter << std::endl;
  }

  os << "banksUsed: ";
  for (i = 0; i < MEM_BANKS; i++)
    os << i << " ";

  os << "\n";

  os << "---End Memory-Controller: " << get_name() << std::endl;
}

bool MemoryController::access_completed_cb(void *arg)
{
    MemoryQueueEntry *queueEntry = (MemoryQueueEntry*)arg;

    bool kernel = queueEntry->request->is_kernel();

    int bank_no = getBankID(queueEntry->request->
            getPhysicalAddress());
    banksUsed[bank_no] = 0;

    N_STAT_UPDATE(new_stats.bank_access, [bank_no]++, kernel);
    switch(queueEntry->request->getType()) {
        case OPERATION_READ:
            N_STAT_UPDATE(new_stats.bank_read, [bank_no]++, kernel);
            break;
        case OPERATION_WRITE:
            N_STAT_UPDATE(new_stats.bank_write, [bank_no]++, kernel);
            break;
        case OPERATION_UPDATE:
            N_STAT_UPDATE(new_stats.bank_update, [bank_no]++, kernel);
            break;
        default:
            assert(0);
    }

    /*
     * Now check if we still have pending requests
     * for the same bank
     */
    MemoryQueueEntry* entry;

    for (auto iter : pendingRequests) {
      entry = iter;

        int bank_no_2 = getBankID(entry->request->
                getPhysicalAddress());
        if(bank_no == bank_no_2 && entry->inUse == false) {
            entry->inUse = true;
            marss_add_event(&accessCompleted,
                    latency, entry);
            banksUsed[bank_no] = 1;
            break;
        }
    }

    if(!queueEntry->annuled) {

        /* Send response back to cache */
        memdebug("Memory access done for Request: ", *queueEntry->request,
                endl);

        wait_interconnect_cb(queueEntry);
    } else {
        queueEntry->request->decRefCounter();
        ADD_HISTORY_REM(queueEntry->request);

        auto iter = std::find(pendingRequests.begin(), pendingRequests.end(), queueEntry);
        if (iter != pendingRequests.end())
          pendingRequests.erase(iter);
    }

    return true;
}

bool MemoryController::wait_interconnect_cb(void *arg)
{
  MemoryQueueEntry *queueEntry = (MemoryQueueEntry*)arg;

  bool success = false;

  /* Don't send response if its a memory update request */
  if(queueEntry->request->getType() == OPERATION_UPDATE) {
    queueEntry->request->decRefCounter();
    ADD_HISTORY_REM(queueEntry->request);

    auto iter = std::find(pendingRequests.begin(), pendingRequests.end(), queueEntry);
    if (iter != pendingRequests.end())
      pendingRequests.erase(iter);
    return true;
  }

  /* First send response of the current request */
  Message& message = *memoryHierarchy_->get_message();
  message.sender = this;
  message.dest = queueEntry->source;
  message.request = queueEntry->request;
  message.hasData = true;

  memdebug("Memory sending message: ", message);
  success = cacheInterconnect->get_controller_request_signal()->
    emit(&message);
  /* Free the message */
  memoryHierarchy_->free_message(&message);

  if(!success) {
    /* Failed to response to cache, retry after 1 cycle */
    marss_add_event(&waitInterconnect, 1, queueEntry);
  } else {
    queueEntry->request->decRefCounter();
    ADD_HISTORY_REM(queueEntry->request);
    auto iter = std::find(pendingRequests.begin(), pendingRequests.end(), queueEntry);
    if (iter != pendingRequests.end())
      pendingRequests.erase(iter);

    if(pendingRequests.size() < MEM_REQ_NUM) {
      memoryHierarchy_->set_controller_full(this, false);
    }
  }
  return true;
}

void MemoryController::annul_request(MemoryRequest *request)
{
    MemoryQueueEntry *queueEntry;
    for(auto iter = pendingRequests.begin();
      iter != pendingRequests.end(); iter++) {
      queueEntry = *iter;

        if(*queueEntry->request == *request) {
            queueEntry->annuled = true;
            if(!queueEntry->inUse) {
                queueEntry->request->decRefCounter();
                ADD_HISTORY_REM(queueEntry->request);
                pendingRequests.erase(iter);
            }
        }
    }
}

int MemoryController::get_no_pending_request(W8 coreid)
{
  int count = 0;
  MemoryQueueEntry *queueEntry;
  for(auto iter : pendingRequests) {
    queueEntry = iter;

    if(queueEntry->request->getCoreId() == coreid)
      count++;
  }
  return count;
}

/**
 * @brief Dump Memory Controller in YAML Format
 *
 * @param out YAML Object
 */
void MemoryController::dump_configuration(YAML::Emitter &out) const
{
  out << YAML::Key << get_name() << YAML::Value << YAML::BeginMap;

  YAML_KEY_VAL(out, "type", "dram_cont");
  YAML_KEY_VAL(out, "RAM_size", ram_size); /* ram_size is from QEMU */
  YAML_KEY_VAL(out, "number_of_banks", MEM_BANKS);
  YAML_KEY_VAL(out, "latency", latency);
  YAML_KEY_VAL(out, "latency_ns", simcycles_to_ns(latency));
  YAML_KEY_VAL(out, "pending_queue_size", pendingRequests.size());

  out << YAML::EndMap;
}

/* Memory Controller Builder */
struct MemoryControllerBuilder : public ControllerBuilder
{
    MemoryControllerBuilder(const char* name) :
        ControllerBuilder(name)
    {}

    Controller* get_new_controller(W8 coreid, W8 type,
            MemoryHierarchy& mem, const char *name) {
        return new MemoryController(coreid, name, &mem);
    }
};

MemoryControllerBuilder memControllerBuilder("simple_dram_cont");
