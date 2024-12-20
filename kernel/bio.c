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
#define HASH(blockno) ((blockno) % NBUCKET)

extern uint ticks;

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // lab8-2,use hash table, per bucket a lock, in the hashtable, buf linked by next
  struct buf buckets[NBUCKET];
  struct spinlock bucket_lock[NBUCKET];
  struct spinlock hash_lock;
  int cur_bufs; // currently used bufs

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
} bcache;

void
binit(void)
{
  struct buf *b;
  initlock(&bcache.lock, "bcache");
  initlock(&bcache.hash_lock,"bcache_hash");
  int i;
  for(i = 0;i < NBUCKET; ++i)
    initlock(&bcache.bucket_lock[i],"bcache_bucket");
  // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    // b->next = bcache.head.next;
    // b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    // bcache.head.next->prev = b;
    // bcache.head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int idx = HASH(blockno);
  struct buf *pre,*minb = 0,*minpre;
  uint min_timestamp;
  int i;
  
  acquire(&bcache.bucket_lock[idx]);
  // look for the block in the bucket
  for(b = bcache.buckets[idx].next;b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket_lock[idx]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  
  // Not cached.
  acquire(&bcache.lock);
  if(bcache.cur_bufs < NBUF){
    // buf is not full, use the next buf in the array, and add it to the corresponding bucket
    b = &bcache.buf[bcache.cur_bufs];
    ++bcache.cur_bufs;
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    // add to the bucket head
    b->next = bcache.buckets[idx].next;
    bcache.buckets[idx].next = b;
    release(&bcache.lock);
    release(&bcache.bucket_lock[idx]);
    acquiresleep(&b->lock);
    return b;
  }
  release(&bcache.lock);
  release(&bcache.bucket_lock[idx]);

  // lru
  acquire(&bcache.hash_lock);
  for(i = 0;i < NBUCKET; ++i){
    min_timestamp = -1;
    acquire(&bcache.bucket_lock[idx]);
    for(pre = &bcache.buckets[idx],b = pre->next;b;pre = b,b = b->next){
       // research the block in the bucket
       if(idx == HASH(blockno) && b->dev == dev && b->blockno == blockno){
        ++b->refcnt;
        release(&bcache.bucket_lock[idx]);
        release(&bcache.hash_lock);
        acquiresleep(&b->lock);
        return b;
       }
       if(b->refcnt == 0 && b->timestamp < min_timestamp){
         min_timestamp = b->timestamp;
         minb = b;
         minpre = pre;
       }
    }
    if(minb){
      minb->dev = dev;
      minb->blockno = blockno;
      minb->valid = 0;
      minb->refcnt = 1;
      if(idx != HASH(blockno)){
        // need to move the buf to the corresponding bucket
        minpre->next = minb->next;
        release(&bcache.bucket_lock[idx]);      
        idx = HASH(blockno);
        acquire(&bcache.bucket_lock[idx]);
        minb->next = bcache.buckets[idx].next;
        bcache.buckets[idx].next = minb;
      }      
      release(&bcache.bucket_lock[idx]);
      release(&bcache.hash_lock);
      acquiresleep(&minb->lock);
      return minb;
    }
    release(&bcache.bucket_lock[idx]);
    if(++idx == NBUCKET)
      idx = 0;
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
  // lab8-2, acquire the lock of the bucket
  int idx = HASH(b->blockno);
  acquire(&bcache.bucket_lock[idx]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    // b->next->prev = b->prev;
    // b->prev->next = b->next;
    // b->next = bcache.head.next;
    // b->prev = &bcache.head;
    // bcache.head.next->prev = b;
    // bcache.head.next = b;

    // only when the buf is released, update the timestamp
    b->timestamp = ticks;
  }
  release(&bcache.bucket_lock[idx]);
}

void
bpin(struct buf *b) {
  int idx = HASH(b->blockno);
  acquire(&bcache.bucket_lock[idx]);
  b->refcnt++;
  release(&bcache.bucket_lock[idx]);
}

void
bunpin(struct buf *b) {
  int idx = HASH(b->blockno);
  acquire(&bcache.bucket_lock[idx]);
  b->refcnt--;
  release(&bcache.bucket_lock[idx]);
}


