#include "../include/buffer_pool.h"
#include "../include/disk_manager.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <stdexcept>


namespace Kintsugi::BufferPool
{
     BufferPoolManager::BufferPoolManager(const std::size_t pool_size, Storage::DiskManager* disk_manager, WAL::WALManager* wal_manager)
          : _disk_manager(disk_manager), _wal_manager(wal_manager) {
               // Sized once here and never resized. That matters: the B-tree holds a
               // Frame* while it works on a node, and a growing vector would move the
               // frames out from under it.
               frames.resize(pool_size);
          }

     std::size_t BufferPoolManager::find_free_or_evictable_frame()
     {
          // frames.size() doubles as "found nothing", since it can never be a real index.
          std::size_t target_idx = frames.size();

          // An untouched slot is free money, no eviction and no write.
          for(std::uint32_t i = 0; i < frames.size(); ++i)
          {
               if(frames[i].page_id == -1)
               {
                    target_idx = i;
                    break;
               }
          }

          if(target_idx == frames.size())
          {
               // Back of the list is the coldest page. Walking backwards means the
               // first unpinned page we hit is the best victim available.
               for(auto it = lru_list.rbegin(); it != lru_list.rend(); ++it)
               {
                    int candidate_page_id = *it;
                    int idx = page_table[candidate_page_id];

                    if(frames[idx].pin_count == 0)
                    {
                         target_idx = idx;
                         break;
                    }
               }
          }

          // Every single page is pinned. Not something a caller can recover from, it
          // means somebody up the stack forgot to unpin, so fail loudly instead of
          // handing back a frame that is still in use.
          if(target_idx == frames.size())
          {
               throw std::runtime_error("Buffer pool full, no evictable page.");
          }

          return target_idx;
     }

     Frame* BufferPoolManager::new_page(int* out_page_id)
     {
          int page_id = _disk_manager->allocate_page();

          std::size_t target_idx = find_free_or_evictable_frame();
          Frame& target_frame = frames[target_idx];

          if(target_frame.page_id != -1)
          {
               // Evicting somebody. Log before writing the page out, that is the whole
               // deal with write-ahead logging: if we crash after the log write and
               // before the disk write, recovery still has the page image.
               if(target_frame.is_dirty)
               {
                    _wal_manager->write_log(target_frame.page_id, target_frame.data);
                    _disk_manager->write_page(target_frame.page_id, target_frame.data);
               }

               page_table.erase(target_frame.page_id);

               auto lru_it = lru_map.find(target_frame.page_id);
               if(lru_it != lru_map.end())
               {
                    lru_list.erase(lru_it->second);
                    lru_map.erase(lru_it);
               }
          }

          // Zero it out, otherwise the caller inherits whatever the last page left
          // behind. The B-tree counts on this, its constructor only sets three fields.
          std::memset(target_frame.data, 0, Storage::DiskManager::PAGE_SIZE);
          target_frame.page_id = page_id;
          target_frame.pin_count = 1;

          // Dirty from the start. The page has no version on disk yet, so it must not
          // be dropped silently even if the caller never writes to it.
          target_frame.is_dirty = true;

          page_table[page_id] = target_idx;
          lru_list.push_front(page_id);
          lru_map[page_id] = lru_list.begin();

          *out_page_id = page_id;

          return &target_frame;
     }

     Frame* BufferPoolManager::fetch_page(int page_id)
     {
          auto it = page_table.find(page_id);

          // Already in memory: pin it, move it to the front and hand it over. No disk
          // access at all, which is the entire point of the pool.
          if(it != page_table.end())
          {
               Frame& frame = frames[it->second];
               frame.pin_count++;

               auto lru_it = lru_map.find(page_id);

               if(lru_it != lru_map.end())
               {
                    lru_list.erase(lru_it->second);
                    lru_list.push_front(page_id);
                    lru_it->second = lru_list.begin();
               }
               else
               {
                    lru_list.push_front(page_id);
                    lru_map[page_id] = lru_list.begin();
               }

               return &frame;
          }

          // Miss. Make room, same eviction and same log-then-write order as new_page.
          std::size_t target_idx = find_free_or_evictable_frame();

          Frame& target = frames[target_idx];

          if(target.page_id != -1)
          {
               if(target.is_dirty)
               {
                    _wal_manager->write_log(target.page_id, target.data);
                    _disk_manager->write_page(target.page_id, target.data);
               }

               page_table.erase(target.page_id);

               auto lru_it = lru_map.find(target.page_id);

               if(lru_it != lru_map.end())
               {
                    lru_list.erase(lru_it->second);
                    lru_map.erase(lru_it);
               }
          }

          // Clean on arrival: this is exactly what the disk holds.
          _disk_manager->read_page(page_id, target.data);
          target.page_id = page_id;
          target.pin_count = 1;
          target.is_dirty = false;

          page_table[page_id] = target_idx;
          lru_list.push_front(page_id);
          lru_map[page_id] = lru_list.begin();

          return &target;
     }

     void BufferPoolManager::unpin_page(int page_id, bool is_dirty)
     {
          // Unpinning a page that was already evicted is not worth complaining about,
          // it can happen on an error path where the caller lost track.
          auto it = page_table.find(page_id);
          if(it == page_table.end()) return;

          Frame& frame = frames[it->second];
          if(frame.pin_count > 0) frame.pin_count--;

          // The flag only ever goes on, never off. One reader passing false must not
          // wipe out the fact that another caller wrote to the page.
          if(is_dirty) frame.is_dirty = true;
     }

     void BufferPoolManager::flush_page(int page_id)
     {
          if(page_id < 0) return;

          // Not resident means nothing to flush, whatever is on disk is current.
          auto it = page_table.find(page_id);
          if(it == page_table.end()) return;

          Frame& frame = frames[it->second];
          if(frame.is_dirty)
          {
               _wal_manager->write_log(page_id, frame.data);
               _disk_manager->write_page(page_id, frame.data);

               // Clean again, but still resident and still pinned if it was pinned.
               // Flushing is not eviction.
               frame.is_dirty = false;
          }
     }

     void BufferPoolManager::flush_all_pages()
     {
          // Pins are ignored here. A pinned page can be mid-edit, so this is only safe
          // at a point where nothing is holding a page, shutdown being the obvious one.
          for(auto& frame : frames)
          {
               if(frame.is_dirty)
               {
                    _wal_manager->write_log(frame.page_id, frame.data);
                    _disk_manager->write_page(frame.page_id, frame.data);
                    frame.is_dirty = false;
               }
          }
     }
}
