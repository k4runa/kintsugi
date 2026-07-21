#include "../include/disk_manager.h"

#include <ios>
#include <iostream>
#include <stdexcept>
#include <string>
#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#endif

namespace Kintsugi::Storage 
{
     DiskManager::DiskManager(const std::string& db_file) : _file_name(db_file)
     {
          // open the file, if fail or not exists, create a new one
#ifdef _WIN32
          _hFile = CreateFile(
               _file_name.c_str(),
               GENERIC_READ | GENERIC_WRITE,
               FILE_SHARE_READ | FILE_SHARE_WRITE,
               NULL,
               OPEN_ALWAYS,
               FILE_ATTRIBUTE_NORMAL,
               NULL
          );  

          if(_hFile == INVALID_HANDLE_VALUE)
          {
               throw std::runtime_error("Could not open file: " + _file_name);
          } 

          std::cout << "INFO: Database created successfully: " << _file_name << std::endl;

#else
          _db_io.open(_file_name, std::ios::in | std::ios::out | std::ios::binary);
     
          //check if file couldn't open, or not exists
          if(!_db_io.is_open())
          {
               std::cerr << "INFO: couldn't found -> " << _file_name
                    << "\nINFO: Creating new database file..." << std::endl;

               _db_io.clear();
               _db_io.open(_file_name, std::ios::out); // just create the file
               _db_io.close();

               //check if created successfully
               _db_io.open(_file_name, std::ios::in | std::ios::out | std::ios::binary);

               if(!_db_io.is_open())
               {
                    throw std::runtime_error("Could not create / open file: " + _file_name);
               }

               std::cout << "INFO: Database created successfully: " << _file_name << std::endl;
          }
#endif //_WIN32
     }

     void DiskManager::read_page(int page_id, char* page_data)
     {
#ifdef _WIN32
          OVERLAPPED overlapped = {0};
          overlapped.Offset = page_id * PAGE_SIZE;
          DWORD bytesRead;

          if(!ReadFile(_hFile, page_data, PAGE_SIZE, &bytesRead, &overlapped))
          {
               throw std::runtime_error("Could not read page, page id: " + std::to_string(page_id));
          }
#else
          _db_io.seekg(static_cast<std::streamoff>(page_id) * PAGE_SIZE);
          _db_io.read(page_data, PAGE_SIZE);

          if(_db_io.fail())
          {
               throw std::runtime_error("Could not read page, page id: " + std::to_string(page_id));
          }

#endif // _WIN32
     }

     void DiskManager::write_page(int page_id, const char* page_data)
     {
#ifdef _WIN32
          OVERLAPPED overlapped = {0};
          overlapped.Offset = page_id * PAGE_SIZE;
          DWORD bytesWritten;

          if(!WriteFile(_hFile, page_data, PAGE_SIZE, &bytesWritten, &overlapped))
          {
               throw std::runtime_error("Could not write to page, page id: " + std::to_string(page_id));
          }

          FlushFileBuffers(_hFile);
#else
          _db_io.seekp(static_cast<std::streamoff>(page_id) * PAGE_SIZE);
          _db_io.write(page_data, PAGE_SIZE);

          if(_db_io.fail())
          {
               throw std::runtime_error("Could not write to page, page id: " + std::to_string(page_id));
          }

          _db_io.flush();
#endif // _WIN32
     }

     DiskManager::~DiskManager()
     {
#ifdef _WIN32
          if(_hFile != INVALID_HANDLE_VALUE)
          {
               CloseHandle(_hFile);
          }
#else
          if(_db_io.is_open())
          {
               _db_io.close();
          }
#endif
     }

     int DiskManager::allocate_page() { return _next_page_id++; }
}
