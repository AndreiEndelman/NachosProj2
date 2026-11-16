// memorymanager.h
//	Hands out free physical pages so several programs can share
//	main memory without overwriting each other.
//	One global instance, memManager, is created in system.cc.

#ifndef MEMORYMANAGER_H
#define MEMORYMANAGER_H

#include "bitmap.h"

class Lock;

class MemoryManager {
  public:
    MemoryManager(int numPages);
    ~MemoryManager();

    int  getPage();            // grab a free physical page, -1 if none left
    void clearPage(int page);  // give a physical page back
    int  NumFreePages();       // how many pages are still free

  private:
    BitMap *pages;
    Lock *lock;
};

#endif // MEMORYMANAGER_H
