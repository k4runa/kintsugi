#pragma once

//includes

#include <cstddef>
#include <fstream>
#include <string>

namespace Kintsugi::Storage 
{
     // Disk manager class
     class DiskManager 
     {
          public:
               static constexpr std::size_t PAGE_SIZE = 4096;
               ~DiskManager() = default;

               // functions
               void ReadPage(int page_id, char* page_data);
               void WritePage(int page_id, const char* page_data);

               int AllocatePage();

          private:
               std::fstream _db_io;
               std::string _file_name;
               int _next_page_id {};
     };
}
