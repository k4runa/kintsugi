#pragma once

#include "btree_node.h"
#include "buffer_pool.h"
#include "key_map.h"
#include "serializer.h"

#include <string>
#include <vector>

namespace Kintsugi 
{
     class Store 
     {
          public:
               Store(Tree::BTreeIndex* _tree, BufferPool::BufferPoolManager* _bpm, Keymap* _key_map);
               void add_entry(const std::string& platform, Serializer::Entry& entry);
               void delete_entry(const std::string& platform, int index);

               Serializer::Entry get_entry(const std::string& platform, int index);
               
               std::vector<Serializer::Entry> list_platform(const std::string& platform);
          
          private:
               Tree::BTreeIndex* tree;
               BufferPool::BufferPoolManager* bpm;
               Keymap* key_map;
     };
}
