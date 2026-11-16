/* forktest.c
 *	Smoke test for Fork before Exit/Yield exist.
 *	The parent forks a child, then spins. The child halts the machine,
 *	which proves it ran on its own (copied) address space.
 *	Run with time slicing so the parent gets preempted:
 *	    ./nachos -rs 5 -x ../test/forktest
 */

#include "syscall.h"

void child()
{
    Halt();
}

int main()
{
    Fork(child);
    while (1)
        ;
}
