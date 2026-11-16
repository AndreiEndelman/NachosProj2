// pcb.cc
//	Process Control Block and the global process table.

#include "pcb.h"
#include "synch.h"

PCB::PCB(PCB *parentProcess, Thread *processThread)
{
    pid = -1;
    parent = parentProcess;
    thread = processThread;
    exitStatus = 0;
    exited = false;
    joinLock = new Lock("join lock");
    joinCond = new Condition("join condition");
}

PCB::~PCB()
{
    delete joinLock;
    delete joinCond;
}

PCBTable::PCBTable()
{
    pids = new BitMap(MAX_PROCESSES);
    pids->Mark(0);   // never hand out pid 0, so the first process is pid 1 (matches the TA's .out files)
    for (int i = 0; i < MAX_PROCESSES; i++)
        table[i] = NULL;
    lock = new Lock("pcb table lock");
}

PCBTable::~PCBTable()
{
    delete pids;
    delete lock;
}

int
PCBTable::Alloc(PCB *pcb)
{
    lock->Acquire();
    int pid = pids->Find();   // lowest free pid, marked as used
    if (pid != -1) {
        table[pid] = pcb;
        pcb->pid = pid;
    }
    lock->Release();
    return pid;
}

void
PCBTable::Free(int pid)
{
    if (pid <= 0 || pid >= MAX_PROCESSES)
        return;
    lock->Acquire();
    if (pids->Test(pid)) {
        pids->Clear(pid);
        table[pid] = NULL;
    }
    lock->Release();
}

PCB *
PCBTable::Get(int pid)
{
    if (pid <= 0 || pid >= MAX_PROCESSES)
        return NULL;
    return table[pid];
}
