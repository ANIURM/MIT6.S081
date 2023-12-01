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
  struct buf buf[NBUF];

  struct buf bufmap[NBUCKET];
  struct spinlock bufmap_lock[NBUCKET];
} bcache;

struct spinlock evict_lock;

void
binit(void)
{
  struct buf *b;

  initlock(&evict_lock, "evict_lock");

  for (int i = 0; i < NBUCKET; i++) { bcache.bufmap[i].next = 0; }

  // Put buffers into bufmap
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    int hash = HASH(b->dev, b->blockno);
    initlock(&bcache.bufmap_lock[hash], "bufmap_lock");
    b->last_use = 0;
    b->next = bcache.bufmap[hash].next;
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
  for (b = bcache.bufmap[hash].next; b != 0; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.bufmap_lock[hash]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Avoid deadlock
  release(&bcache.bufmap_lock[hash]);

  acquire(&evict_lock);

  // Check Again:
  // Is the block already cached?
  for (b = bcache.bufmap[hash].next; b != 0; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&evict_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Find a buffer to evict.
  // Trick: record the pointer before lru_buf for easier deletion.
  struct buf *before_lru_buf = 0;
  int holding_lock = -1;
  for (int i = 0; i < NBUCKET; i++) {
    int new_found = 0;
    acquire(&bcache.bufmap_lock[i]);
    for (b = &bcache.bufmap[i]; b->next != 0; b = b->next) {
      if (b->next->refcnt == 0) {
        if (before_lru_buf == 0 || b->next->last_use < before_lru_buf->last_use) {
          before_lru_buf = b;
          holding_lock = i;
          new_found = 1;
        }
      }
    }
    if (new_found) {
      if (holding_lock != -1)
        release(&bcache.bufmap_lock[holding_lock]);
      // keep holding the lock where new lru_buf is found.
      holding_lock = i;
    } else {
      release(&bcache.bufmap_lock[i]);
    }
  }

  if (before_lru_buf == 0) {
    panic("bget: no buffers");
  }

  // Evict the buffer.
  b = before_lru_buf->next;
  // Remove lru_buf from original bucket
  before_lru_buf->next = b->next;
  // Add lru_buf to the head of the bucket
  b->next = bcache.bufmap[hash].next;
  bcache.bufmap[hash].next = b;
  // Update buf info
  b->blockno = blockno;
  b->dev = dev;
  b->valid = 0;
  b->refcnt = 1;
  b->last_use = ticks;
  acquiresleep(&b->lock);
  release(&bcache.bufmap_lock[holding_lock]);
  release(&evict_lock);
  
  return b;
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

  int hash = HASH(b->dev, b->blockno);
  acquire(&bcache.bufmap_lock[hash]);
  b->refcnt--;
  b->last_use = ticks;
  
  release(&bcache.bufmap_lock[hash]);
}

void
bpin(struct buf *b) {
  int hash = HASH(b->dev, b->blockno);
  acquire(&bcache.bufmap_lock[hash]);
  b->refcnt++;
  release(&bcache.bufmap_lock[hash]);
}

void
bunpin(struct buf *b) {
  int hash = HASH(b->dev, b->blockno);
  acquire(&bcache.bufmap_lock[hash]);
  b->refcnt--;
  release(&bcache.bufmap_lock[hash]);
}


