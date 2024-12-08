// lab5. cow struct
// for pyhsical page, maintain a ref cnt and a lock
#include "types.h"
#include "param.h"
#include "riscv.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"


struct cow{
    int refs;
    struct spinlock lock;
};

// cow array, each element corresponds to a physical page
struct cow cows[(PHYSTOP - KERNBASE) >> 12];

int incref(uint64 pa){
    if(pa < KERNBASE){ 
        return -1;
    }
    pa = (pa-KERNBASE) >> 12;
    acquire(&cows[pa].lock);
    ++cows[pa].refs;
    release(&cows[pa].lock);
    return 0;
}

int decref(uint64 pa){
    int cnt;
    if(pa < KERNBASE){
           return -1;
    }   
    pa = (pa-KERNBASE) >> 12;
    acquire(&cows[pa].lock);
    cnt = --cows[pa].refs;
    release(&cows[pa].lock);
    return cnt;
}
