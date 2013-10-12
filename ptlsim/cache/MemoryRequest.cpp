
/*
 * MARSSx86 : A Full System Computer-Architecture Simulator
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Copyright 2009 Avadh Patel <apatel@cs.binghamton.edu>
 * Copyright 2009 Furat Afram <fafram@cs.binghamton.edu>
 *
 */

#ifdef MEM_TEST
#include <test.h>
#else
#include <globals.h>
#include <ptlsim.h>
#define PTLSIM_PUBLIC_ONLY
#include <ptlhwdef.h>
#endif

#include <statelist.h>
#include <memoryHierarchy.h>

#include "MemoryRequest.h"
#include "PoolAllocator.h"

using namespace Memory;

// Request pool allocator; quickly allocate/move/release requests.
PoolAllocator <MemoryRequest, 1024> MemoryRequest::Allocator;

void MemoryRequest::init(W8 coreId,
		W8 threadId,
		W64 physicalAddress,
		int robId,
		W64 cycles,
		bool isInstruction,
		W64 ownerRIP,
		W64 ownerUUID,
		OperationType opType)
{
	coreId_ = coreId;
	threadId_ = threadId;
	physicalAddress_ = physicalAddress;
	robId_ = robId;
	cycles_ = cycles;
	ownerRIP_ = ownerRIP;
	ownerUUID_ = ownerUUID;
	refCounter_ = 0; // or maybe 1
	operationType = opType;
	isData_ = !isInstruction;

	if(history) delete history;
	history = new stringbuf();

	memdebug("Init ", *this, endl);
}

void MemoryRequest::init(MemoryRequest *request)
{
	coreId_ = request->coreId_;
	threadId_ = request->threadId_;
	physicalAddress_ = request->physicalAddress_;
	robId_ = request->robId_;
	cycles_ = request->cycles_;
	ownerRIP_ = request->ownerRIP_;
	ownerUUID_ = request->ownerUUID_;
	refCounter_ = 0; // or maybe 1
	operationType = request->getType();
	isData_ = request->isData_;

	if(history) delete history;
	history = new stringbuf();

	memdebug("Init ", *this, endl);
}

bool MemoryRequest::is_same(W8 coreid,
		W8 threadid,
		int robid,
		W64 physaddr,
		bool is_icache,
		bool is_write)
{
	OperationType type;
	if(is_write)
		type = OPERATION_WRITE;
	else
		type = OPERATION_READ;
	if(coreId_ == coreid &&
			threadId_ == threadid &&
			robId_ == robid &&
			physicalAddress_ == physaddr &&
			isData_ == !is_icache &&
			getType() == type)
		return true;
	return false;
}

bool MemoryRequest::is_same(MemoryRequest *request)
{
	if(coreId_ == request->coreId_ &&
			threadId_ == request->threadId_ &&
			robId_ == request->robId_ &&
			physicalAddress_ == request->physicalAddress_ &&
			isData_ == request->isData_ &&
			getType() == request->getType())
		return true;
	return false;
}

