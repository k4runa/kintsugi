#include "../include/key_map.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace Kintsugi
{
     // File layout, plain text so it can be read with an editor when something looks off:
     //
     //     <next_id>
     //     <key> <id>
     //     <key> <id>
     //     ...
     //
     // next_id sits on the first line because it has to survive on its own. Taking the
     // highest id in the file and adding one would start reusing ids as soon as the
     // last key gets removed.
     Keymap::Keymap(const std::string& _file_path) : file_path(_file_path)
     {
          // Touch the file first so the fstream below finds something to open.
          std::ofstream create(file_path, std::ios::app);
          create.close();
          file_io.open(file_path, std::ios::in | std::ios::out);

          if(!file_io.is_open())
          {
               throw std::runtime_error("Could not open file: " + file_path);
          }

          // On a brand new, empty file this extraction fails and leaves next_id at 0,
          // which is the value we want anyway. The stream is in a fail state after
          // that, so the loop below reads nothing and we start with an empty map.
          file_io >> next_id;
          std::string key; int id;

          // Keys cannot contain whitespace, which is what makes >> enough here.
          while(file_io >> key >> id)
          {
               map.insert({key, id});
          }
     }

     Keymap::~Keymap()
     {
          // The whole table is rewritten from scratch rather than patched in place.
          // Entries change size when a key is removed, so there is no way to edit one
          // line of a text file without moving everything after it anyway.
          //
          // Nothing is written before this point, so a crash mid-run loses every key
          // added since startup. That is the price of keeping the map purely in memory.
          if(file_io.is_open())
          {
               file_io.close();
          }

          std::ofstream out(file_path, std::ios::trunc);
          out << next_id << std::endl;
          for(auto& [key, id] : map)
          {
               out << key << " " << id << std::endl;
          }
     }

     int Keymap::get_or_create(const std::string& key)
     {
          auto it = map.find(key);

          if(it != map.end())
          {
               return it->second; // id
          }

          int id = next_id++;
          map.insert({key, id});
          return id;
     }

     int Keymap::get(const std::string& key)
     {
          auto it = map.find(key);

          if(it != map.end())
          {
               return it->second;
          }

          // -1 and not an exception: callers ask about keys that may not exist as a
          // normal part of their work, and ids are never negative so it cannot collide.
          return -1;
     }

     // Prefix match, so this answers "how many keys start with this" and not "how many
     // entries does this platform have". Those two differ the moment a platform name is
     // a prefix of another one, gmail and gmail2 for instance. Kept as is because it is
     // still useful for rough questions, but do not build indexes on it.
     int Keymap::count(const std::string& key)
     {
          int count = 0;

          for(auto& [k, v] : map)
          {
               if(k.starts_with(key)) count++;
          }

          return count;
     }

     // Highest index in use for this platform, plus one. Linear in the size of the map,
     // which is fine at this scale and saves us from storing a counter per platform in
     // the file.
     //
     // Indexes are not compacted after a delete: remove gmail:0 out of 0,1,2 and the
     // next add still gets 3. Handing out 0 again would be safe for the tree, but the
     // user just saw entry 0 disappear and would find a different one in its place.
     int Keymap::next_index(const std::string& platform)
     {
          int next = 0;

          for(auto& [k, v] : map)
          {
               std::size_t sep = k.find(':');
               if(sep == std::string::npos) continue;
               if(k.compare(0, sep, platform) != 0) continue; // exact platform, not a prefix

               int index = std::stoi(k.substr(sep + 1));
               if(index >= next) next = index + 1;
          }

          return next;
     }

     std::unordered_map<std::string, int> Keymap::get_map()
     {
          return map;
     }

     // Throws instead of returning false, because every caller reaches this after
     // looking the key up, so a miss here means the caller is confused about its
     // own state and quietly ignoring that would leave the tree and the map disagreeing.
     void Keymap::remove(const std::string& key)
     {
          auto it = map.find(key);
          if(it == map.end()) throw std::runtime_error("Key not found: " + key);
          map.erase(it);
     }
}
