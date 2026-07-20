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
               DiskManager(const std::string& db_file);
               ~DiskManager() = default;

               // functions
               void read_page(int page_id, char* page_data);
               void write_page(int page_id, const char* page_data);

               int allocate_page();

          private:
               std::fstream _db_io;
               std::string _file_name;
               int _next_page_id {};
     };
}
