#include "../include/btree_node.h"
#include "../include/buffer_pool.h"
#include "../include/key_map.h"
#include "../include/serializer.h"
#include "../include/store.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <strings.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Kintsugi
{
     Store::Store(Tree::BTreeIndex* _tree, BufferPool::BufferPoolManager* _bpm, Keymap* _key_map) : tree(_tree), bpm(_bpm), key_map(_key_map)
     {
          //Nothing here
     }

     void Store::add_entry(const std::string& platform, Serializer::Entry& entry)
     {
          // The page is filled before the key exists anywhere, so if serializing or
          // writing blows up, nothing points at the half-written page.
          std::vector<std::uint8_t> buf = Serializer::serialize(entry);
          int page_id = -1;
          BufferPool::Frame* frame = bpm->new_page(&page_id);

          // new_page hands back a zeroed frame, so the rest of the page stays zero and
          // deserialize stops after the fields it was told about.
          //
          // An entry bigger than a page would run off the end of the frame here. Nothing
          // stops that yet, it just has not happened with the field sizes we store.
          std::memcpy(frame->data, buf.data(), buf.size());
          bpm->unpin_page(page_id, true);

          int index = key_map->next_index(platform);
          std::string k = platform + ":" + std::to_string(index);
          int key = key_map->get_or_create(k);

          // next_index() should have given us a free slot, so a refused insert means a
          // key exists in the tree that the keymap does not know about. Take the keymap
          // entry back out rather than leaving a name pointing at nothing, and make
          // noise, because at that point the two are already out of step.
          if(!tree->insert(key, page_id))
          {
               key_map->remove(k);
               throw std::runtime_error("Could not index entry: " + k);
          }
     }

     Serializer::Entry Store::get_entry(const std::string& platform, int index)
     {
          std::string k = platform + ":" + std::to_string(index);
          int key = key_map->get(k);
          if(key == -1) throw std::runtime_error("Entry not found: " + k);
          int page_id = -1;

          // The keymap knowing the name but the tree not knowing the key is a different
          // failure from "no such entry", so it gets its own message. Seeing this one
          // means the two went out of sync somewhere.
          bool f = tree->search(key, &page_id); if(!f) throw std::runtime_error("Key not in tree: " + k);

          // Copy the page out and unpin straight away instead of deserializing off the
          // frame. Holding a pin across the parsing would keep a slot of the pool busy
          // for no reason, and the copy is one page.
          BufferPool::Frame* frame = bpm->fetch_page(page_id);
          std::vector<std::uint8_t> buf(frame->data, frame->data + Storage::DiskManager::PAGE_SIZE);
          bpm->unpin_page(page_id, false);

          std::uint32_t offset = 0;
          return Serializer::deserialize(buf, offset);
     }

     void Store::delete_entry(const std::string& platform, int index)
     {
          std::string k = platform + ":" + std::to_string(index);
          int key = key_map->get(k);
          if(key == -1) throw std::runtime_error("Entry not found: " + k);

          int page_id = -1;
          bool found = tree->search(key, &page_id);
          if(!found) throw std::runtime_error("Key not in tree: " + k);

          // Wipe the page instead of only dropping the key. This is a password store:
          // an unreferenced page still sits in the file, and anyone reading the raw
          // file would find the old entry there. Marked dirty so the zeroes are what
          // eventually reaches the disk.
          BufferPool::Frame* frame = bpm->fetch_page(page_id);
          std::memset(frame->data, 0, Storage::DiskManager::PAGE_SIZE);
          bpm->unpin_page(page_id, true);

          // Page id is not returned to anyone, the file keeps it. A free list is the
          // fix when the wasted space starts to matter.
          tree->delete_k(key);
          key_map->remove(k);
     }

     std::vector<Serializer::Entry> Store::list_platform(const std::string& platform)
     {
          std::unordered_map<std::string, int> map = key_map->get_map();
          std::vector<int> indexes;
          for(auto& [k, v] : map)
          {
               std::size_t sep = k.find(':');
               if(sep == std::string::npos) continue;
               if(k.compare(0, sep, platform) != 0) continue; // exact platform, not a prefix

               indexes.push_back(std::stoi(k.substr(sep + 1)));
          }

          // the keymap is unordered, so sort before reading the entries
          //
          // Indexes get collected and sorted first, then the pages are read. Sorting
          // whole Entry objects afterwards would move strings around for nothing, and
          // this way the page reads go in ascending order too.
          std::sort(indexes.begin(), indexes.end());

          std::vector<Serializer::Entry> vec;
          vec.reserve(indexes.size());
          for(int idx : indexes)
          {
               // Walks the tree again per entry. Wasteful, but list_platform runs on a
               // handful of entries and the alternative is a second lookup path.
               vec.push_back(get_entry(platform, idx));
          }

          return vec;
     }

     std::vector<std::string> Store::list_platforms()
     {
          // There is one key per entry, so a platform holding five entries turns up
          // five times in the map. The set is what collapses those down to a name,
          // the vector is only here because that is what the caller wants back.
          std::unordered_map<std::string, int> map = key_map->get_map();
          std::unordered_set<std::string> tmp;
          std::vector<std::string> vec;

          for(auto& [k, v] : map)
          {
               // A key with no ':' has no index part to cut off, and find() coming back
               // as npos makes substr take the whole string. list_platform() drops those
               // instead. add_entry() never writes one, only a direct keymap call can.
               std::string platform = k.substr(0, k.find(':'));
               tmp.insert(platform);
          }

          for(auto& v : tmp)
          {
               vec.push_back(v);
          }

          // Both containers above are unordered, so without this the names come out in
          // hash order: not the order they were added, not alphabetical, and liable to
          // rearrange themselves as soon as a new platform changes the bucket count.
          // Sorting names is cheap, there are as many of them as the user has platforms.
          std::sort(vec.begin(), vec.end());
          return vec;
     }
}
