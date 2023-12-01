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

struct kmem {
  struct spinlock lock;
  struct run *freelist;
  char lockname[8];
};

struct kmem kmems[NCPU];

void
kinit()
{
  for (int i = 0; i < NCPU; i++) {
    snprintf(kmems[i].lockname, sizeof(kmems[i].lockname), "kmem_%d", i);
    initlock(&kmems[i].lock, kmems[i].lockname);
  }
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

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  struct kmem *kmem_ptr = &kmems[cpuid()];
  pop_off();
  acquire(&kmem_ptr->lock);
  r->next = kmem_ptr->freelist;
  kmem_ptr->freelist = r;
  release(&kmem_ptr->lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int id = cpuid();
  struct kmem *kmem_ptr = &kmems[id];
  pop_off();
  acquire(&kmem_ptr->lock);

  // no free memory in current CPU's free list
  // steal 64 pages from other CPUs
  if (kmem_ptr->freelist == 0) {
    int steal_left = 64;
    for (int i = 0; i < NCPU; i++) {
      if (i == id) continue;
      acquire(&kmems[i].lock);
      while (kmems[i].freelist && steal_left > 0) {
        r = kmems[i].freelist;
        kmems[i].freelist = r->next;
        r->next = kmem_ptr->freelist;
        kmem_ptr->freelist = r;
        steal_left--;
      }
      release(&kmems[i].lock);
      if (steal_left == 0) break;
    }
  }

  r = kmem_ptr->freelist;
  if(r)
    kmem_ptr->freelist = r->next;
  release(&kmem_ptr->lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
