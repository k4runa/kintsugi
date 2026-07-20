#include "../include/buffer_pool.h"
#include "../include/disk_manager.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <stdexcept>


namespace Kintsugi::BufferPool 
{
     BufferPoolManager::BufferPoolManager(const std::size_t pool_size, Storage::DiskManager* disk_manager)
          : _disk_manager(disk_manager) {
               frames.resize(pool_size);
          }

     std::size_t BufferPoolManager::find_free_or_evictable_frame()
     {
          std::size_t target_idx = frames.size();

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
               if(target_frame.is_dirty)
               {
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

          std::memset(target_frame.data, 0, Storage::DiskManager::PAGE_SIZE);
          target_frame.page_id = page_id;
          target_frame.pin_count = 1;
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

          std::size_t target_idx = find_free_or_evictable_frame();

          Frame& target = frames[target_idx];

          if(target.page_id != -1)
          {
               if(target.is_dirty)
               {
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
          auto it = page_table.find(page_id);
          if(it == page_table.end()) return;

          Frame& frame = frames[it->second];
          if(frame.pin_count > 0) frame.pin_count--;
          if(is_dirty) frame.is_dirty = true;
     }

     void BufferPoolManager::flush_page(int page_id)
     {
          if(page_id < 0) return;

          auto it = page_table.find(page_id);
          if(it == page_table.end()) return;

          Frame& frame = frames[it->second];
          if(frame.is_dirty)
          {
               _disk_manager->write_page(page_id, frame.data);
               frame.is_dirty = false;
          }
     }

     void BufferPoolManager::flush_all_pages()
     {
          for(auto& frame : frames)
          {
               if(frame.is_dirty)
               {
                    _disk_manager->write_page(frame.page_id, frame.data);
                    frame.is_dirty = false;
               }
          }
     }
}
