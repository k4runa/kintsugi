#include "../include/disk_manager.h"

#include <ios>
#include <iostream>
#include <stdexcept>
#include <string>

namespace Kintsugi::Storage 
{
     DiskManager::DiskManager(const std::string& db_file) : _file_name(db_file)
     {
          // open the file, if fail or not exists, create a new one.
          _db_io.open(_file_name, std::ios::in | std::ios::out | std::ios::binary);
     
          //check if file couldn't open, or not exists
          if(!_db_io.is_open())
          {
               std::cerr << "WARNING: couldn't found -> " << _file_name
                    << "\nCreating..." << std::endl;

               _db_io.clear();
               _db_io.open(_file_name, std::ios::out); // just create the file
               _db_io.close();

               //check if created successfully
               _db_io.open(_file_name, std::ios::in | std::ios::out | std::ios::binary);

               if(!_db_io.is_open())
               {
                    throw std::runtime_error("Could not create / open file: " + _file_name);
               }

               std::cout << "Database created successfully: " << _file_name << std::endl;
          }
     }

     void DiskManager::read_page(int page_id, char* page_data)
     {
          _db_io.seekg(static_cast<std::streamoff>(page_id) * PAGE_SIZE);
          _db_io.read(page_data, PAGE_SIZE);

          if(_db_io.fail())
          {
               throw std::runtime_error("Could not read page, page id: " + std::to_string(page_id));
          }
     }

     void DiskManager::write_page(int page_id, const char* page_data)
     {
          _db_io.seekg(static_cast<std::streamoff>(page_id) * PAGE_SIZE);
          _db_io.write(page_data, PAGE_SIZE);

          if(_db_io.fail())
          {
               throw std::runtime_error("Could not write to page, page id: " + std::to_string(page_id));
          }

          _db_io.flush();
     }

     int DiskManager::allocate_page() { return _next_page_id++; }
}
