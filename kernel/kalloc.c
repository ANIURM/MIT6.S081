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
  struct kmem kmem_now = kmems[cpuid()];
  pop_off();
  acquire(&kmem_now.lock);
  r->next = kmem_now.freelist;
  kmem_now.freelist = r;
  release(&kmem_now.lock);
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
  struct kmem kmem_now = kmems[id];
  pop_off();
  acquire(&kmem_now.lock);
  r = kmem_now.freelist;
  if(r)
    kmem_now.freelist = r->next;
  release(&kmem_now.lock);

  // no free memory in current CPU's free list
  // steal from other CPUs
  if (r == 0) {
    for (int i = 0; i < NCPU; i++) {
      if (i == id) continue;
      acquire(&kmems[i].lock);
      r = kmems[i].freelist;
      if (r) {
        kmems[i].freelist = r->next;
        release(&kmems[i].lock);
        break;
      }
      release(&kmems[i].lock);
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
