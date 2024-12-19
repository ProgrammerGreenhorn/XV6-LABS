// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct cpu_freelist {
  struct spinlock lock;
  struct run *freelist;
  char lock_name[8];
} kmems[NCPU];// lab8-1, per cpu a freelist


void
kinit()
{ int i;
  // lab 8-1
  for(i = 0;i < NCPU;++i){
    snprintf(kmems[i].lock_name, 8,"kmem_d%d", i);
    initlock(&kmems[i].lock,kmems[i].lock_name);
  }
  // because the kinit is called by the cpu 0,
  // so we give all memory to it, and when other
  // cpu need memory, they came to steal from cpu 0's freelist
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int id;
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  // lab8-1 turn off interrupt,give the free mem to the current cpu's freelist 
  push_off();
  id = cpuid();
  pop_off();
  acquire(&kmems[id].lock);
  r->next = kmems[id].freelist;
  kmems[id].freelist = r;
  release(&kmems[id].lock);
}

struct run *steal(int id){
  int i;
  int c = id;
  struct run *fast,*slow,*head;
  if(cpuid() != id)
    panic("steal");
  for(i = 1; i < NCPU; ++i){
    // c is the current cpu wo try to steal
    c = (c + 1) % NCPU;
    acquire(&kmems[c].lock);
    if(kmems[c].freelist){
      // steal the before half from it
      slow = head = kmems[c].freelist;
      fast = slow->next;
      while(fast && fast->next){
        fast = fast->next->next;
        slow = slow->next;
      }
      kmems[c].freelist = slow->next;
      release(&kmems[c].lock);
      slow->next = 0;
      return head;
    }
    release(&kmems[c].lock);
  }
  return 0;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int id;
  push_off();
  id = cpuid();
  pop_off();
  acquire(&kmems[id].lock);
  r = kmems[id].freelist;
  if(r)
    kmems[id].freelist = r->next;
  release(&kmems[id].lock);
  // lab 8-1 steal page from other cpu
  if(!r && (r = steal(id))){
    acquire(&kmems[id].lock);
    kmems[id].freelist = r->next;
    release(&kmems[id].lock);
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk

  return (void*)r;
}

