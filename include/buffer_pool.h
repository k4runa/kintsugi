#pragma once

#include "disk_manager.h"
#include <cstddef>

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

          private:
               Storage::DiskManager* _disk_manager = nullptr;
     };
}
