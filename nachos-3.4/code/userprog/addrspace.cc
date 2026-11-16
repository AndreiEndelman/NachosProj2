// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#ifdef CHANGED
#include "memorymanager.h"
#endif
#include "noff.h"
#ifdef HOST_SPARC
#include <strings.h>
#endif

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable)
{
    NoffHeader noffH;
    unsigned int i, size;

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

#ifdef CHANGED
    // Only take pages if the whole program fits in what's left.
    if (numPages > (unsigned int)memManager->NumFreePages()) {
	DEBUG('a', "Not enough free memory for %d pages\n", numPages);
	valid = FALSE;
	numPages = 0;
	pageTable = NULL;
	return;
    }
    valid = TRUE;

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
					numPages, size);
    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
	pageTable[i].virtualPage = i;
	pageTable[i].physicalPage = memManager->getPage();
	ASSERT(pageTable[i].physicalPage >= 0);
	pageTable[i].valid = TRUE;
	pageTable[i].use = FALSE;
	pageTable[i].dirty = FALSE;
	pageTable[i].readOnly = FALSE;
	// zero this page only (other programs live in the rest of memory)
	bzero(&(machine->mainMemory[pageTable[i].physicalPage * PageSize]),
	      PageSize);
    }

    // copy in the code and data segments, page by page
    if (noffH.code.size > 0) {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
			noffH.code.virtualAddr, noffH.code.size);
	LoadSegment(executable, noffH.code.virtualAddr, noffH.code.size,
		    noffH.code.inFileAddr);
    }
    if (noffH.initData.size > 0) {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
			noffH.initData.virtualAddr, noffH.initData.size);
	LoadSegment(executable, noffH.initData.virtualAddr,
		    noffH.initData.size, noffH.initData.inFileAddr);
    }
}

//----------------------------------------------------------------------
// AddrSpace::LoadSegment
//	Read "size" bytes from the file into user memory starting at
//	virtual address "virtAddr". The physical pages aren't contiguous,
//	so copy at most up to the end of each page at a time.
//----------------------------------------------------------------------

void
AddrSpace::LoadSegment(OpenFile *executable, int virtAddr, int size,
		       int inFileAddr)
{
    while (size > 0) {
	int physAddr;
	bool ok = Translate(virtAddr, &physAddr);
	ASSERT(ok);
	int chunk = PageSize - (virtAddr % PageSize);	// room left in page
	if (chunk > size)
	    chunk = size;
	executable->ReadAt(&(machine->mainMemory[physAddr]), chunk, inFileAddr);
	virtAddr += chunk;
	inFileAddr += chunk;
	size -= chunk;
    }
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace(AddrSpace *parent)
//	Used by Fork. Give the child its own physical pages and copy the
//	parent's memory into them, so parent and child can't overwrite each
//	other. The caller must check there are enough free pages first.
//----------------------------------------------------------------------

AddrSpace::AddrSpace(AddrSpace *parent)
{
    numPages = parent->numPages;
    valid = TRUE;
    pageTable = new TranslationEntry[numPages];
    for (unsigned int i = 0; i < numPages; i++) {
	pageTable[i] = parent->pageTable[i];		// copy the flags
	pageTable[i].physicalPage = memManager->getPage();	// new frame
	ASSERT(pageTable[i].physicalPage >= 0);
	bcopy(&(machine->mainMemory[parent->pageTable[i].physicalPage * PageSize]),
	      &(machine->mainMemory[pageTable[i].physicalPage * PageSize]),
	      PageSize);
    }
}

//----------------------------------------------------------------------
// AddrSpace::Translate
//	Turn a user virtual address into a physical address in mainMemory.
//	Returns FALSE if the address is outside this address space.
//----------------------------------------------------------------------

bool
AddrSpace::Translate(int virtAddr, int *physAddr)
{
    if (virtAddr < 0)
	return FALSE;
    unsigned int vpn = (unsigned int) virtAddr / PageSize;
    if (vpn >= numPages || !pageTable[vpn].valid)
	return FALSE;
    *physAddr = pageTable[vpn].physicalPage * PageSize + (virtAddr % PageSize);
    return TRUE;
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Give this address space's physical pages back to the memory manager.
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
    for (unsigned int i = 0; i < numPages; i++)
	memManager->clearPage(pageTable[i].physicalPage);
    delete [] pageTable;
}
#else
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

    ASSERT(numPages <= NumPhysPages);		// check we're not trying
						// to run anything too big --
						// at least until we have
						// virtual memory

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
					numPages, size);
// first, set up the translation 
    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
	pageTable[i].virtualPage = i;	// for now, virtual page # = phys page #
	pageTable[i].physicalPage = i;
	pageTable[i].valid = TRUE;
	pageTable[i].use = FALSE;
	pageTable[i].dirty = FALSE;
	pageTable[i].readOnly = FALSE;  // if the code segment was entirely on 
					// a separate page, we could set its 
					// pages to be read-only
    }
    
// zero out the entire address space, to zero the unitialized data segment 
// and the stack segment
    bzero(machine->mainMemory, size);

// then, copy in the code and data segments into memory
    if (noffH.code.size > 0) {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
			noffH.code.virtualAddr, noffH.code.size);
        executable->ReadAt(&(machine->mainMemory[noffH.code.virtualAddr]),
			noffH.code.size, noffH.code.inFileAddr);
    }
    if (noffH.initData.size > 0) {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
			noffH.initData.virtualAddr, noffH.initData.size);
        executable->ReadAt(&(machine->mainMemory[noffH.initData.virtualAddr]),
			noffH.initData.size, noffH.initData.inFileAddr);
    }

}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
   delete pageTable;
}

#endif // CHANGED

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}
