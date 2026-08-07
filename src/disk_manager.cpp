#include "../include/disk_manager.h"

#include <ios>
#include <iostream>
#include <stdexcept>
#include <string>

namespace Kintsugi::Storage
{
     DiskManager::DiskManager(const std::string& db_file, const bool _show_logs = false) : _file_name(db_file), show_logs(_show_logs)
     {
          // open the file, if fail or not exists, create a new one.
          // Has to be binary, otherwise Windows would happily turn a 0x0A inside a
          // page into a newline pair and every offset after it would shift.
          _db_io.open(_file_name, std::ios::in | std::ios::out | std::ios::binary);

          //check if file couldn't open, or not exists
          if(!_db_io.is_open())
          {
               // in|out will not create a missing file, so open it once with out only
               // to bring it into existence, then reopen it the way we actually want.
              
               if(show_logs)
               {
                    std::cerr << "INFO: couldn't found -> " << _file_name
                         << "\nINFO: Creating new database file..." << std::endl;
               }

               _db_io.clear();
               _db_io.open(_file_name, std::ios::out); // just create the file
               _db_io.close();

               //check if created successfully
               _db_io.open(_file_name, std::ios::in | std::ios::out | std::ios::binary);

               if(!_db_io.is_open())
               {
                    throw std::runtime_error("Could not create / open file: " + _file_name);
               }
               
               if(show_logs)
               {
                    std::cout << "INFO: Database created successfully: " << _file_name << std::endl;
               }
          }
     }

     DiskManager::~DiskManager()
     {
          if(_db_io.is_open())
          {
               _db_io.close();
          }
     }

     void DiskManager::read_page(int page_id, char* page_data)
     {
          _db_io.seekg(static_cast<std::streamoff>(page_id) * PAGE_SIZE);
          _db_io.read(page_data, PAGE_SIZE);

          // Reading a page that was never written lands past the end of the file and
          // trips this. Usually it means somebody handed us a page id we never gave out.
          if(_db_io.fail())
          {
               throw std::runtime_error("Could not read page, page id: " + std::to_string(page_id));
          }
     }

     void DiskManager::write_page(int page_id, const char* page_data)
     {
          // seekg and not seekp, which reads odd but works: a filebuf keeps a single
          // position for reading and writing, so both seeks move the same thing.
          _db_io.seekg(static_cast<std::streamoff>(page_id) * PAGE_SIZE);
          _db_io.write(page_data, PAGE_SIZE);

          if(_db_io.fail())
          {
               throw std::runtime_error("Could not write to page, page id: " + std::to_string(page_id));
          }

          // Push it out of the stream buffer now. This still only gets it as far as
          // the OS cache, a real durability guarantee would need fsync.
          _db_io.flush();
     }

     // Pages are handed out in order and never given back. Deleting an entry wipes its
     // page but the id stays gone, so the file only grows.
     //
     // Worse, the counter starts at 0 every run because nothing writes it down. Reopen
     // an existing database and the first allocation hands out page 0 again, right on
     // top of the old root. Nothing here survives a restart yet, so it has not bitten
     // us, but this is the thing to fix first when it should.
     int DiskManager::allocate_page() { return _next_page_id++; }
}
