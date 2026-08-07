#pragma once

#include "btree_node.h"
#include "buffer_pool.h"
#include "key_map.h"
#include "serializer.h"

#include <string>
#include <vector>

namespace Kintsugi
{
     // Where the pieces meet. Everything below this deals in page ids and int keys,
     // and this is the layer that lets you say "give me the second gmail account".
     //
     // One lookup goes: "gmail:1" -> keymap gives an int id -> the tree gives a page
     // id -> the buffer pool gives the page -> the serializer turns it into an Entry.
     //
     // An entry gets a page to itself. A 4 KB page for a couple hundred bytes of
     // password is wasteful, but it means a delete is a single memset and nothing has
     // to be shuffled around inside the page.
     //
     // Store borrows the three pointers, it does not own them. They have to outlive it.
     class Store
     {
          public:
               Store(Tree::BTreeIndex* _tree, BufferPool::BufferPoolManager* _bpm, Keymap* _key_map);

               // The index is per platform and starts at 0. add_entry picks the next
               // free one for you, the others expect an index you already know about.
               void add_entry(const std::string& platform, Serializer::Entry& entry);
               void delete_entry(const std::string& platform, int index);

               Serializer::Entry get_entry(const std::string& platform, int index);

               // Sorted by index. Empty when the platform has nothing stored.
               std::vector<Serializer::Entry> list_platform(const std::string& platform);

               // Every platform that has at least one entry, each named once, sorted
               // by name. Empty when nothing is stored at all.
               std::vector<std::string> list_platforms();

          private:
               Tree::BTreeIndex* tree;
               BufferPool::BufferPoolManager* bpm;
               Keymap* key_map;
     };
}
