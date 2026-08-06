#pragma once

// includes
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "buffer_pool.h"
#include "disk_manager.h"

namespace Kintsugi::Tree
{
     // Tree Node, Tree Structure
     //
     // A node is a page, nothing more. We never allocate one of these: a page comes
     // back from the buffer pool as raw bytes and we reinterpret_cast the frame data
     // to BTreeNode*, so the layout below *is* the on-disk format. Which means: no
     // virtuals, no pointers, no std::string, nothing that stores an address. Only
     // page ids, because an address means nothing after a restart.
     class BTreeNode
     {
          public:
               // 509 is not a round number, it is the largest count that still fits a
               // node in one page. Run the numbers: 12 bytes of header, keys[510] is
               // 2040, children[511] is 2044, which lands on 4096 exactly. See the
               // static_assert below, it is what keeps this honest if the fields change.
               static constexpr int MAX_KEYS = 509;

               // Below this a node has to borrow from a sibling or merge with one,
               // that is what keeps the tree from degenerating into a linked list.
               static constexpr int MIN_KEYS = MAX_KEYS / 2;

               //define other node fields
               bool is_leaf;
               int key_count;

               // Leaves are chained left to right so a range scan can walk them
               // without going back up through the parents. -1 means end of the chain.
               int next_leaf_page_id;

               // One slot more than MAX_KEYS on purpose. Insert drops the key in first
               // and splits afterwards, so the node is allowed to sit one key over the
               // limit for a moment.
               int keys[MAX_KEYS + 1];

               // A node is either a leaf or an internal node, never both, so the two
               // arrays share the space. Read the right one or you get nonsense.
               union {
                    int values[MAX_KEYS + 1]; // if the node is leaf
                    int children[MAX_KEYS + 2]; // if is internal
               };

               // Declared, never defined, and never called. Nodes are only ever
               // reached by casting a page, so there is nothing for a constructor to
               // do. Calling it would fail at link time.
               BTreeNode();
     };

     //be sure the node size is not larger than the page size.
     static_assert(sizeof(BTreeNode) <= Kintsugi::Storage::DiskManager::PAGE_SIZE, "Node size is too large.");

     // int -> int index on top of the buffer pool. Store uses it to go from a key id
     // to the page holding an entry.
     //
     // The tree owns no memory. Every operation fetches pages, works on them and
     // unpins them again, so the pool stays in charge of what is resident.
     class BTreeIndex
     {
          public:
               BTreeIndex(Kintsugi::BufferPool::BufferPoolManager* bpm);

               // define functions
               bool insert(int key, int value);          // false if the key is already there
               bool search(int key, int* out_value);
               bool delete_k(int key);                   // false if there was nothing to delete
               bool update(int key, int new_value);      // false if the key does not exist

               std::vector<std::pair<int, int>> range_query(std::uint32_t min, std::uint32_t max) const;

          private:
               Kintsugi::BufferPool::BufferPoolManager* _bpm = nullptr;

               // Moves when the root splits or the tree shrinks. Also only lives in
               // memory, so a fresh BTreeIndex always starts from an empty root.
               int _root_page_id;

               //private functions
               void insert_into_leaf(BTreeNode* node, int key, int value);
               void insert_into_internal(BTreeNode*, int middle_key, int right_page_id);

               // Both return the page id of the new right half and hand back the key
               // that has to be pushed up into the parent.
               int split_leaf(int left_page_id, BTreeNode* left, int* out_middle_key);
               int split_internal(BTreeNode* left, int* out_middle_key);
     };
}
