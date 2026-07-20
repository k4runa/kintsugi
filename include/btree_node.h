#pragma once

// includes
#include <string>
#include "buffer_pool.h"
#include "disk_manager.h"

namespace Kintsugi::Tree 
{
     // Tree Node, Tree Structure
     class BTreeNode 
     {
          public:
               //define min and max keys
               static constexpr int MAX_KEYS = 509;
               static constexpr int MIN_KEYS = MAX_KEYS / 2;

               //define other node fields
               bool is_leaf;
               int key_count;
               int next_leaf_page_id;
               int keys[MAX_KEYS + 1];

               union {
                    int values[MAX_KEYS + 1]; // if the node is leaf
                    int children[MAX_KEYS + 2]; // if is internal
               };

               BTreeNode();
     };

     //be sure the node size is not larger than the page size.
     static_assert(sizeof(BTreeNode) <= Kintsugi::Storage::DiskManager::PAGE_SIZE, "Node size is too large.");

     class BTreeIndex
     {
          public:
               BTreeIndex(Kintsugi::BufferPool::BufferPoolManager* bpm);
          
               // define functions
               bool insert(int key, int value);
               bool search(int key, int* out_value);

               void insert_into_leaf(BTreeNode* node, int key, int value);
               void insert_into_internal(BTreeNode*, int middle_key, int right_page_id);

               int split_leaf(int left_page_id, BTreeNode* left, int* out_middle_key);
               int split_internal(BTreeNode* left, int* out_middle_key);

          private:
               Kintsugi::BufferPool::BufferPoolManager* _bpm = nullptr;
               int _root_page_id;
     };
}
