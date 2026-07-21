#pragma once

//includes

#include <cstddef>
#include <fstream>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h> 
#endif // _WIN32

namespace Kintsugi::Storage 
{
     // Disk manager class
     class DiskManager 
     {
          public:
               static constexpr std::size_t PAGE_SIZE = 4096;
               DiskManager(const std::string& db_file);
               ~DiskManager();

               // functions
               void read_page(int page_id, char* page_data);
               void write_page(int page_id, const char* page_data);

               int allocate_page();

          private:
#ifdef _WIN32
               HANDLE _hFile = INVALID_HANDLE_VALUE;
#else 
               std::fstream _db_io;
#endif
               std::string _file_name;
               int _next_page_id {};
     };
}
