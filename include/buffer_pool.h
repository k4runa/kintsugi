#pragma once

#include "disk_manager.h"
#include <cstddef>
#include <list>
#include <unordered_map>
#include <vector>

namespace Kintsugi::BufferPool
{
     // Frame class
     class Frame
     {
          public:
               char data[Storage::DiskManager::PAGE_SIZE];
               int page_id = -1; //empty
               bool is_dirty = false;
               int pin_count = 0;
          
          private:
               //nothing for now.
     };

     // BufferPoolManager class
     class BufferPoolManager
     {
          public:
               BufferPoolManager(std::size_t pool_size, Storage::DiskManager* disk_manager);

               //define functions
               Frame* fetch_page(int page_id);
               Frame* new_page(int* out_page_id);

               void unpin_page(int page_id, bool is_dirty);
               void flush_page(int page_id);
               void flush_all_pages();

               std::size_t find_free_or_evictable_frame();

          private:
               Storage::DiskManager* _disk_manager = nullptr;

               std::list<int> lru_list;
               std::unordered_map<int, std::list<int>::iterator> lru_map;
               std::vector<Frame> frames;
               std::unordered_map<int,int> page_table;
     };
}
