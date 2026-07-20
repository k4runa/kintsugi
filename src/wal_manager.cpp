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
          record.pagae_id = page_id;
          record.lsn = next_lsn++;
          record.type = WALType::UPDATE;

          std::memcpy(record.new_data, new_data, Storage::DiskManager::PAGE_SIZE);

          log_io.seekp(0, std::ios::end);
          log_io.write(reinterpret_cast<const char*>(&record), sizeof(WALRecord));

          if(log_io.fail())
          {
               throw std::runtime_error("Could not read to file: " + _file_name);
          }

          log_io.flush();

          return record.lsn;
     }

     void WALManager::recover(Storage::DiskManager* disk_manager)
     {
          WALRecord record;
          
          log_io.seekg(0, std::ios::beg);
          
          while(log_io.read(reinterpret_cast<char*>(&record), sizeof(WALRecord)))
          {
               disk_manager->write_page(record.pagae_id, record.new_data);
          }
     }

     void WALManager::clear()
     {
          log_io.close();
          log_io.open(_file_name, std::ios::out | std::ios::trunc);
          log_io.close();
          log_io.open(_file_name, std::ios::in | std::ios::out | std::ios::binary);
          next_lsn = 0;
     }
}
