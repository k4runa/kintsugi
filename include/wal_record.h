#pragma once

#include "disk_manager.h"

#include <cstddef>
#include <fstream>
#include <string>

namespace Kintsugi::WAL
{
     // COMMIT and ABORT are here for when transactions land. Right now every record
     // written is an UPDATE.
     enum class WALType {UPDATE, COMMIT, ABORT};

     // One record per page write, and the record carries the whole 4 KB image of the
     // page rather than "these bytes changed". That wastes space, but replaying is
     // then just a page write, and replaying the same record twice cannot corrupt
     // anything. With byte diffs we would need an undo image to get that property.
     //
     // The struct is dumped to disk as raw bytes, padding included, so the log file
     // only makes sense to a build with this exact layout. Fine for a local file,
     // not something to copy between machines.
     struct WALRecord
     {
          std::size_t lsn;       // grows by one per record, restarts at 0 each run
          std::size_t page_id;

          WALType type;

          char new_data[Storage::DiskManager::PAGE_SIZE];
     };

     // Write-ahead logging: the buffer pool hands a dirty page here before it writes
     // that page to the database file. If we die between the two, the change is still
     // in the log and recover() can put it back. The order is the whole point, the log
     // write has to hit the disk first, otherwise it buys us nothing.
     class WALManager
     {
          public:
               WALManager(const std::string& log_file);
               ~WALManager() = default;

               // Appends one record and returns its lsn.
               std::size_t write_log(int page_id, const char* new_data);

               // Replays the log over the database file, oldest record first. Nobody
               // calls this yet, it has to run on startup before anything reads a page.
               void recover(Storage::DiskManager* disk_manager);

               // Throws the log away. Only safe once every logged page is on disk for real.
               void clear();

          private:
               std::fstream log_io;
               std::string _file_name;
               std::size_t next_lsn = 0;
     };
}
