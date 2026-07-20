//include source files
#include "../include/buffer_pool.h"
#include "../include/disk_manager.h"
#include "../include/btree_node.h"


#ifndef POOL_SIZE
#define POOL_SIZE 10

#endif

int main() 
{
    Kintsugi::Storage::DiskManager           disk_manager("database.db");
    Kintsugi::WAL::WALManager                wal_manager("logs.wal");
    Kintsugi::BufferPool::BufferPoolManager  buffer_pool(POOL_SIZE, &disk_manager, &wal_manager);
    Kintsugi::Tree::BTreeIndex               tree(&buffer_pool);

    return 0;
}
