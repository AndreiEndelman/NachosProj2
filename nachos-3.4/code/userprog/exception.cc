// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

#ifdef CHANGED
#include "pcb.h"
#include "memorymanager.h"

//----------------------------------------------------------------------
// AdvancePC
//	Move the user program past the syscall instruction. Without this
//	the program re-runs the same syscall forever.
//----------------------------------------------------------------------

static void
AdvancePC()
{
    machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
    machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
    machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg) + 4);
}

//----------------------------------------------------------------------
// ForkStart
//	The first thing a forked child's kernel thread runs. A brand-new
//	thread never goes through the restore code in Scheduler::Run(), so
//	load the child's registers and page table here, then start running
//	user code.
//----------------------------------------------------------------------

static void
ForkStart(int unused)
{
    currentThread->RestoreUserState();
    currentThread->space->RestoreState();
    machine->Run();		// never returns
    ASSERT(FALSE);
}

//----------------------------------------------------------------------
// doFork
//	Fork(func): make a child process with a copy of the parent's
//	memory that starts running at "func". Returns the child's pid to
//	the parent, or -1 if there's no room for it.
//----------------------------------------------------------------------

static int
doFork(int funcAddr)
{
    PCB *parentPcb = currentThread->pcb;
    AddrSpace *parentSpace = currentThread->space;

    // pid first: the "not enough memory" message prints the would-be pid
    PCB *childPcb = new PCB(parentPcb, NULL);
    int childPid = pcbTable->Alloc(childPcb);
    if (childPid == -1) {
	delete childPcb;
	return -1;
    }

    // Check there's room before taking any pages
    if ((unsigned int)memManager->NumFreePages() < parentSpace->getNumPages()) {
	printf("Not Enough Memory for Child Process %d\n", childPid);
	pcbTable->Free(childPid);
	delete childPcb;
	return -1;
    }

    Thread *child = new Thread("forked child");
    child->space = new AddrSpace(parentSpace);	// copy of parent's memory
    child->pcb = childPcb;
    childPcb->thread = child;

    // Child starts with the parent's registers (stack pointer, return
    // address, ...), but at funcAddr. When func returns, the child lands
    // right after the Fork() call in the parent's code.
    child->SaveUserState();
    child->SetUserRegister(PCReg, funcAddr);
    child->SetUserRegister(NextPCReg, funcAddr + 4);
    child->SetUserRegister(PrevPCReg, funcAddr - 4);

    printf("Process [%d] Fork: start at address [0x%x] with [%d] pages memory\n",
	   parentPcb->pid, funcAddr, child->space->getNumPages());

    child->Fork(ForkStart, 0);	// put the child on the ready list
    return childPid;
}
#endif // CHANGED

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);

#ifdef CHANGED
    if (which == SyscallException) {
	switch (type) {
	case SC_Halt:
	    DEBUG('a', "Shutdown, initiated by user program.\n");
	    interrupt->Halt();
	    break;

	case SC_Fork:
	    printf("System Call: [%d] invoked Fork.\n", currentThread->pcb->pid);
	    machine->WriteRegister(2, doFork(machine->ReadRegister(4)));
	    AdvancePC();
	    break;

	// Andrei: add SC_Yield, SC_Exit, SC_Join, SC_Kill here.
	// Alec:   add SC_Exec here.
	// Pattern: read args from r4 (r5, r6...), write the return value
	// to r2, and call AdvancePC() before break (except Exit/Halt,
	// which never return to the program).

	default:
	    printf("Unexpected system call %d\n", type);
	    ASSERT(FALSE);
	}
    } else {
	printf("Unexpected user mode exception %d %d\n", which, type);
	ASSERT(FALSE);
    }
#else
    if ((which == SyscallException) && (type == SC_Halt)) {
	DEBUG('a', "Shutdown, initiated by user program.\n");
   	interrupt->Halt();
    } else {
	printf("Unexpected user mode exception %d %d\n", which, type);
	ASSERT(FALSE);
    }
#endif
}
