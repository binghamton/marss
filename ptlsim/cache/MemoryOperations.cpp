// ============================================================================
//  MemoryOperations.h: Memory operation types.
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
#include "MemoryOperations.h"

namespace Memory {

//
// Define the memory operation types.
// See "MemoryOperations.md" for a listing.
//
const char *OperationNames[NUM_OPERATION_TYPES] = {
#define X(op) #op,
#include "MemoryOperations.md"
#undef X
};

}

