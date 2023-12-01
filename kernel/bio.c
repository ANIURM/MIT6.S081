// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13
#define HASH(dev, blockno) (((uint)dev ^ (uint)blockno) % NBUCKET)

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  struct buf bufmap[NBUCKET];
  struct spinlock bufmap_lock[NBUCKET];
} bcache;

struct spinlock evict_lock;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  initlock(&evict_lock, "evict_lock");

  for (int i = 0; i < NBUCKET; i++) {
    bcache.bufmap[i].next = &bcache.bufmap[i];
    bcache.bufmap[i].prev = &bcache.bufmap[i];
  }

  // Put buffers into bufmap
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    int hash = HASH(b->dev, b->blockno);
    initlock(&bcache.bufmap_lock[hash], "bufmap_lock");
    b->last_use = 0;
    b->next = bcache.bufmap[hash].next;
    b->prev = &bcache.bufmap[hash];
    bcache.bufmap[hash].next->prev = b;
    bcache.bufmap[hash].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int hash = HASH(dev, blockno);
  acquire(&bcache.bufmap_lock[hash]);

  // Is the block already cached?
  for (b = bcache.bufmap[hash].next; b != &bcache.bufmap[hash]; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.bufmap_lock[hash]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  
  release(&bcache.lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


