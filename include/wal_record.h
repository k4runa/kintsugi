#pragma once

#include "disk_manager.h"

#include <cstddef>
#include <fstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Kintsugi::WAL 
{
     enum class WALType {UPDATE, COMMIT, ABORT};

     struct WALRecord
     {
          std::size_t lsn;
          std::size_t page_id;

          WALType type;

          char new_data[Storage::DiskManager::PAGE_SIZE];
     };

     class WALManager
     {
          public:
               WALManager(const std::string& log_file);
               ~WALManager();

               std::size_t write_log(int page_id, const char* new_data);

               void recover(Storage::DiskManager* disk_manager);
               void clear();

          private:
#ifdef _WIN32
               HANDLE _hFile = INVALID_HANDLE_VALUE;
#else
               std::fstream log_io;
#endif // _WIN32
               std::string _file_name;
               std::size_t next_lsn = 0;
     };
}
