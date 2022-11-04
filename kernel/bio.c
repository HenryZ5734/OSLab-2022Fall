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

#define NBUCKETS 13

struct {
  struct spinlock lock[NBUCKETS];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[NBUCKETS];
} bcache;

void
binit(void)
{
  struct buf *b;

  for(int i=0;i<NBUCKETS;i++){
    initlock(&bcache.lock[i], "bcache");

    // Create linked list of buffers
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
  }
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head[0].next;
    b->prev = &bcache.head[0];
    initsleeplock(&b->lock, "buffer");
    bcache.head[0].next->prev = b;
    bcache.head[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int hash_value = blockno % NBUCKETS;

  acquire(&bcache.lock[hash_value]);

  // Is the block already cached?
  for(b = bcache.head[hash_value].next; b != &bcache.head[hash_value]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[hash_value]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.head[hash_value].prev; b != &bcache.head[hash_value]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[hash_value]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // get buf from other bucket
  int new_bucket = hash_value;
  do{
    new_bucket = (new_bucket+1) % NBUCKETS;
    acquire(&bcache.lock[new_bucket]);
    for(b = bcache.head[new_bucket].prev; b != &bcache.head[new_bucket]; b = b->prev){
      if(b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        // concat new_bucket
        b->next->prev = b->prev;
        b->prev->next = b->next;
        release(&bcache.lock[new_bucket]);
        // concat old_bucket
        b->next = bcache.head[hash_value].next;
        b->prev = &bcache.head[hash_value];
        bcache.head[hash_value].next->prev = b;
        bcache.head[hash_value].next = b;
        release(&bcache.lock[hash_value]);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.lock[new_bucket]);
  }while(new_bucket != hash_value);

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
  int hash_value = b->blockno % NBUCKETS;
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock[hash_value]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head[hash_value].next;
    b->prev = &bcache.head[hash_value];
    bcache.head[hash_value].next->prev = b;
    bcache.head[hash_value].next = b;
  }
  
  release(&bcache.lock[hash_value]);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock[b->blockno%NBUCKETS]);
  b->refcnt++;
  release(&bcache.lock[b->blockno%NBUCKETS]);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock[b->blockno%NBUCKETS]);
  b->refcnt--;
  release(&bcache.lock[b->blockno%NBUCKETS]);
}


