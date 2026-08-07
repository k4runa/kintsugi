#pragma once

//includes

#include <cstddef>
#include <fstream>
#include <string>

namespace Kintsugi::Storage
{
     // The only place in the project that touches the database file. Everything above
     // it thinks in page ids, and this class is what turns a page id into a file offset.
     //
     // The page is the unit for the whole engine: the buffer pool caches whole pages,
     // a B-tree node is exactly one page, and the WAL logs one page per record. 4 KB
     // because that is what the disk gives us anyway, writing less is not any faster.
     class DiskManager
     {
          public:
               static constexpr std::size_t PAGE_SIZE = 4096;

               DiskManager(const std::string& db_file, const bool _show_logs);
               ~DiskManager();

               // functions
               // Both of these want a buffer of exactly PAGE_SIZE bytes. Give them
               // anything shorter and they read or write past the end of it.
               void read_page(int page_id, char* page_data);
               void write_page(int page_id, const char* page_data);

               // Next unused page id. Counts from zero on every run, see the .cpp
               int allocate_page();

          private:
               std::fstream _db_io;
               std::string _file_name;

               bool show_logs;

               // Not stored anywhere, only lives as long as the process does.
               int _next_page_id {};
     };
}
