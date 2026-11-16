// pcb.h
//	Process Control Block, plus the global table that hands out pids.
//	One global instance, pcbTable, is created in system.cc.

#ifndef PCB_H
#define PCB_H

#include "bitmap.h"

class Thread;
class Lock;
class Condition;

#define MAX_PROCESSES 32

class PCB {
  public:
    PCB(PCB *parentProcess, Thread *processThread);
    ~PCB();

    int pid;              // set by PCBTable::Alloc
    PCB *parent;          // NULL for the first process
    Thread *thread;       // kernel thread running this process

    // For Exit / Join (Andrei)
    int exitStatus;
    bool exited;
    Lock *joinLock;
    Condition *joinCond;
};

class PCBTable {
  public:
    PCBTable();
    ~PCBTable();

    int  Alloc(PCB *pcb);   // lowest free pid >= 1, or -1 if the table is full
    void Free(int pid);
    PCB *Get(int pid);      // NULL if the pid is out of range or unused

  private:
    BitMap *pids;
    PCB *table[MAX_PROCESSES];
    Lock *lock;
};

#endif // PCB_H
