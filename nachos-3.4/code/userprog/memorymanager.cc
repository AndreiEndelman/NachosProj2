// memorymanager.cc
//	Physical page allocator: a bitmap over main memory, behind a lock.

#include "memorymanager.h"
#include "synch.h"

MemoryManager::MemoryManager(int numPages)
{
    pages = new BitMap(numPages);
    lock = new Lock("memory manager lock");
}

MemoryManager::~MemoryManager()
{
    delete pages;
    delete lock;
}

int
MemoryManager::getPage()
{
    lock->Acquire();
    int page = pages->Find();   // lowest free page, marked as used; -1 if full
    lock->Release();
    return page;
}

void
MemoryManager::clearPage(int page)
{
    lock->Acquire();
    if (page >= 0 && pages->Test(page))
        pages->Clear(page);
    lock->Release();
}

int
MemoryManager::NumFreePages()
{
    lock->Acquire();
    int n = pages->NumClear();
    lock->Release();
    return n;
}
