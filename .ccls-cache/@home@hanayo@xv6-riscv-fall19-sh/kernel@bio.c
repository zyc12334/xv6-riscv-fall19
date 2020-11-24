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
  // head.next is most recently used.
  //struct buf head;
  struct buf hashbuckets[NBUCKETS];
} bcache;

int 
bhash(int id)
{
  return id % NBUCKETS;
}

void
binit(void)
{
  struct buf *b;
  for(int i = 0; i < NBUCKETS; i++){
    initlock(&bcache.lock[i], "bcache.buckets");
    b = &bcache.hashbuckets[i];
    b->prev = b;
    b->next = b;
  }
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.hashbuckets[0].next;
    b->prev = &bcache.hashbuckets[0];
    initsleeplock(&b->lock, "buffer");
    bcache.hashbuckets[0].next->prev = b;
    bcache.hashbuckets[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int id = bhash(blockno);
  acquire(&bcache.lock[id]);

  // Is the block already cached?
  for(b = bcache.hashbuckets[id].next; b != &bcache.hashbuckets[id]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[id]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached; recycle an unused buffer.

  int new_id = (id + 1) % NBUCKETS;
  while(new_id != id){
    acquire(&bcache.lock[new_id]); 
    for(b = bcache.hashbuckets[new_id].prev; b != &bcache.hashbuckets[new_id]; b = b->prev){
      if(b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        b->next->prev = b->prev;
        b->prev->next = b->next;
        release(&bcache.lock[new_id]);
        b->next = bcache.hashbuckets[id].next;
        b->prev = &bcache.hashbuckets[id];
        bcache.hashbuckets[id].next->prev = b;
        bcache.hashbuckets[id].next = b;
        
        release(&bcache.lock[id]);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.lock[new_id]);
    new_id = (new_id + 1) % NBUCKETS;
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b->dev, b, 0);
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
  virtio_disk_rw(b->dev, b, 1);
}

// Release a locked buffer.
// Move to the head of the MRU list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  int id = bhash(b->blockno);
  acquire(&bcache.lock[id]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbuckets[id].next;
    b->prev = &bcache.hashbuckets[id];
    bcache.hashbuckets[id].next->prev = b;
    bcache.hashbuckets[id].next = b;
  }
  
  release(&bcache.lock[id]);
}

void
bpin(struct buf *b) {
  int id = bhash(b->blockno);
  acquire(&bcache.lock[id]);
  b->refcnt++;
  release(&bcache.lock[id]);
}

void
bunpin(struct buf *b) {
  int id = bhash(b->blockno);
  acquire(&bcache.lock[id]);
  b->refcnt--;
  release(&bcache.lock[id]);
}


