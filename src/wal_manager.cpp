#include "../include/wal_record.h"
#include "../include/disk_manager.h"

#include <cstddef>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>


namespace Kintsugi::WAL
{
     WALManager::WALManager(const std::string& log_file) : _file_name(log_file)
     {
          // open the file, if fail or not exists, create a new one.
          // Same two-step dance as DiskManager: in|out refuses to create the file.
          log_io.open(log_file, std::ios::in | std::ios::out | std::ios::binary);

          //check if file couldn't open, or not exists
          if(!log_io.is_open())
          {
               std::cerr << "INFO: No log file found. \nINFO: Creating a new log file..." << std::endl;
               log_io.clear();
               log_io.open(log_file, std::ios::out);
               log_io.close();

               //check if created successfully
               log_io.open(log_file, std::ios::in | std::ios::out | std::ios::binary);

               if(!log_io.is_open())
               {
                    throw std::runtime_error("Could not open / create file: " + log_file);
               }

               std::cout << "INFO: Log file created successfully: " << log_file << std::endl;
          }
     }

     std::size_t WALManager::write_log(int page_id, const char* new_data)
     {
          WALRecord record;
          record.page_id = page_id;
          record.lsn = next_lsn++;
          record.type = WALType::UPDATE;

          std::memcpy(record.new_data, new_data, Storage::DiskManager::PAGE_SIZE);

          // Always append. The log is read back front to back, so order on disk is
          // what tells recovery which write came last.
          log_io.seekp(0, std::ios::end);
          log_io.write(reinterpret_cast<const char*>(&record), sizeof(WALRecord));

          if(log_io.fail())
          {
               throw std::runtime_error("Could not read to file: " + _file_name);
          }

          // The flush is the reason any of this helps. Without it the record could sit
          // in the stream buffer while the page it protects is already on disk, which
          // is exactly the order we are trying to avoid.
          log_io.flush();

          return record.lsn;
     }

     void WALManager::recover(Storage::DiskManager* disk_manager)
     {
          WALRecord record;

          log_io.seekg(0, std::ios::beg);

          // Oldest to newest, and every record is replayed whether or not the page
          // already made it to disk. Rewriting a page with the image it already holds
          // costs a write and changes nothing, which is why no checks are needed here.
          // A half-written trailing record just fails the read and ends the loop.
          while(log_io.read(reinterpret_cast<char*>(&record), sizeof(WALRecord)))
          {
               disk_manager->write_page(record.page_id, record.new_data);
          }
     }

     void WALManager::clear()
     {
          // No way to truncate an open fstream, so close it, reopen with trunc to drop
          // the contents, then reopen in the read/write mode the rest of the class wants.
          log_io.close();
          log_io.open(_file_name, std::ios::out | std::ios::trunc);
          log_io.close();
          log_io.open(_file_name, std::ios::in | std::ios::out | std::ios::binary);
          next_lsn = 0;
     }
}
