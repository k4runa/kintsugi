#include "../include/btree_node.h"
#include "../include/buffer_pool.h"
#include "../include/key_map.h"
#include "../include/serializer.h"
#include "../include/store.h"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <strings.h>
#include <unordered_map>
#include <vector>

namespace Kintsugi 
{
     Store::Store(Tree::BTreeIndex* _tree, BufferPool::BufferPoolManager* _bpm, Keymap* _key_map) : tree(_tree), bpm(_bpm), key_map(_key_map)
     {

     }

     void Store::add_entry(const std::string& platform, Serializer::Entry& entry)
     {
          std::vector<std::uint8_t> buf = Serializer::serialize(entry);
          int page_id = -1;
          BufferPool::Frame* frame = bpm->new_page(&page_id);
          std::memcpy(frame->data, buf.data(), buf.size());
          bpm->unpin_page(page_id, true);
          int index = key_map->count(platform);
          std::string k = platform + ":" + std::to_string(index);
          int key = key_map->get_or_create(k);
          tree->insert(key, page_id);
     }

     Serializer::Entry Store::get_entry(const std::string& platform, int index)
     {
          std::string k = platform + ":" + std::to_string(index);
          int key = key_map->get(platform);
          if(key == -1) throw std::runtime_error("Entry not found: " + k);
          int page_id = -1;
          bool f = tree->search(key, &page_id); if(!f) throw std::runtime_error("Key not in tree: " + k);

          BufferPool::Frame* frame = bpm->fetch_page(page_id);
          std::vector<std::uint8_t> buf(frame->data, frame->data + Storage::DiskManager::PAGE_SIZE);
          bpm->unpin_page(page_id, false);

          std::uint32_t offset = 0;
          return Serializer::deserialize(buf, offset);
     }

     void Store::delete_entry(const std::string& platform, int index)
     {
          //later later later LATER
     }

     std::vector<Serializer::Entry> Store::list_platform(const std::string& platform)
     {
          std::unordered_map<std::string, int> map = key_map->get_map();
          std::vector<Serializer::Entry> vec;
          for(auto& [k, v] : map)
          {
               int idx = std::stoi(k.substr(k.find(':') + 1));
               if(k.starts_with(platform)) vec.push_back(get_entry(platform, idx));
          }

          return vec;
     }
}
