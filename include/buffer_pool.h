#pragma once

#include "disk_manager.h"
#include "wal_record.h"

#include <cstddef>
#include <list>
#include <unordered_map>
#include <vector>

namespace Kintsugi::BufferPool
{
     // A frame is one slot of page-sized memory. Pages move in and out of it, the
     // frame itself stays put for the life of the pool, which is why callers can hold
     // a Frame* while they work on the page.
     struct Frame
     {
          char data[Storage::DiskManager::PAGE_SIZE];
          int page_id = -1;      // -1 means the slot is empty
          bool is_dirty = false; // memory and disk disagree, do not drop it without writing
          int pin_count = 0;     // how many callers are using the page right now
     };

     // The cache between the B-tree and the disk. It keeps a fixed number of pages in
     // memory and decides which one to throw out when it runs out of room.
     //
     // Everything that reads a page has to unpin it afterwards. A frame with a pin on
     // it can never be evicted, so a forgotten unpin slowly eats the pool until
     // find_free_or_evictable_frame() has nothing left and throws.
     class BufferPoolManager
     {
          public:
               BufferPoolManager(std::size_t pool_size, Storage::DiskManager* disk_manager, WAL::WALManager* wal_manager);

               //define functions

               // Both return a frame that is already pinned for you.
               Frame* fetch_page(int page_id);   // from memory if it is there, from disk otherwise
               Frame* new_page(int* out_page_id);

               // Say whether you dirtied the page. Passing false when you did write to
               // it loses the write, so when in doubt pass true.
               void unpin_page(int page_id, bool is_dirty);

               void flush_page(int page_id);
               void flush_all_pages();

               // Free slot first, otherwise the least recently used unpinned one.
               std::size_t find_free_or_evictable_frame();

          private:
               Storage::DiskManager* _disk_manager = nullptr;
               WAL::WALManager*      _wal_manager  = nullptr;

               // LRU bookkeeping. Front of the list is the most recently touched page.
               // The map is there so we can pull a page out of the middle in O(1),
               // which is the reason for a list and not a vector.
               std::list<int> lru_list;
               std::unordered_map<int, std::list<int>::iterator> lru_map;

               std::vector<Frame> frames;

               // page id -> index into frames
               std::unordered_map<int,int> page_table;
     };
}
