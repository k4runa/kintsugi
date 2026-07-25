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
#ifdef _WIN32
          // OPEN_ALWAYS: opens if exists, creates if not. Either way we end up
          // with a valid handle positioned at offset 0.
          _hFile = CreateFileA(
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
               throw std::runtime_error("Could not open/create file: " + _file_name);
          }
          std::cout << "INFO: Log file opened successfully: " << log_file << std::endl;
#else
          log_io.open(log_file, std::ios::in | std::ios::out | std::ios::binary);
          if(!log_io.is_open())
          {
               std::cerr << "INFO: No log file found. \nINFO: Creating a new log file..." << std::endl;
               log_io.clear();
               log_io.open(log_file, std::ios::out);
               log_io.close();
               log_io.open(log_file, std::ios::in | std::ios::out | std::ios::binary);
               if(!log_io.is_open())
               {
                    throw std::runtime_error("Could not open / create file: " + log_file);
               }
               std::cout << "INFO: Log file created successfully: " << log_file << std::endl;
          }
#endif // _WIN32
     }

     WALManager::~WALManager()
     {
#ifdef _WIN32
          if(_hFile != INVALID_HANDLE_VALUE)
          {
               CloseHandle(_hFile);
          }
#else
          if(log_io.is_open())
          {
               log_io.close();
          }
#endif // _WIN32
     }

     std::size_t WALManager::write_log(int page_id, const char* new_data)
     {
          WALRecord record{}; // zero-init: kills padding garbage before it hits disk
          record.page_id = page_id;
          record.lsn = next_lsn++;
          record.type = WALType::UPDATE;
          std::memcpy(record.new_data, new_data, Storage::DiskManager::PAGE_SIZE);

#ifdef _WIN32
          // Append-only, same as the POSIX path: always seek to real EOF first,
          // then a plain synchronous WriteFile (no OVERLAPPED - we don't need
          // async I/O here, and mixing FILE_END seek with an OVERLAPPED.Offset
          // was the original bug: OVERLAPPED.Offset always wins over the file
          // pointer, so it never actually appended).
          LARGE_INTEGER zero{};
          zero.QuadPart = 0;
          if(!SetFilePointerEx(_hFile, zero, NULL, FILE_END))
          {
               throw std::runtime_error("Could not seek to end of file: " + _file_name);
          }

          DWORD bytesWritten = 0;
          if(!WriteFile(_hFile, &record, static_cast<DWORD>(sizeof(WALRecord)), &bytesWritten, NULL)
             || bytesWritten != sizeof(WALRecord))
          {
               throw std::runtime_error("Could not write WAL record, page id: " + std::to_string(page_id));
          }

          FlushFileBuffers(_hFile);
          return record.lsn;
#else
          log_io.seekp(0, std::ios::end);
          log_io.write(reinterpret_cast<const char*>(&record), sizeof(WALRecord));
          if(log_io.fail())
          {
               throw std::runtime_error("Could not write file: " + _file_name);
          }
          log_io.flush();
          return record.lsn;
#endif // _WIN32
     }

     void WALManager::recover(Storage::DiskManager* disk_manager)
     {
          WALRecord record{};

#ifdef _WIN32
          LARGE_INTEGER zero{};
          zero.QuadPart = 0;
          if(!SetFilePointerEx(_hFile, zero, NULL, FILE_BEGIN))
          {
               throw std::runtime_error("Could not seek to start of file: " + _file_name);
          }

          DWORD bytesRead = 0;
          while(ReadFile(_hFile, &record, static_cast<DWORD>(sizeof(WALRecord)), &bytesRead, NULL)
                && bytesRead == sizeof(WALRecord))
          {
               disk_manager->write_page(record.page_id, record.new_data);
          }
#else
          log_io.clear();          // reset eof/fail bits from any prior operation
          log_io.seekg(0, std::ios::beg);

          while(log_io.read(reinterpret_cast<char*>(&record), sizeof(WALRecord)))
          {
               disk_manager->write_page(record.page_id, record.new_data);
          }
          log_io.clear();          // leave stream usable after hitting eof
#endif // _WIN32
     }

     void WALManager::clear()
     {
#ifdef _WIN32
          LARGE_INTEGER zero{};
          zero.QuadPart = 0;
          if(!SetFilePointerEx(_hFile, zero, NULL, FILE_BEGIN))
          {
               throw std::runtime_error("Could not seek to start of file: " + _file_name);
          }
          if(!SetEndOfFile(_hFile))
          {
               throw std::runtime_error("Could not truncate file: " + _file_name);
          }
#else
          log_io.close();
          log_io.open(_file_name, std::ios::out | std::ios::trunc);
          log_io.close();
          log_io.open(_file_name, std::ios::in | std::ios::out | std::ios::binary);
          if(!log_io.is_open())
          {
               throw std::runtime_error("Could not reopen file after clear: " + _file_name);
          }
#endif // _WIN32
          next_lsn = 0;
     }
}
