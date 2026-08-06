#pragma once

#include <fstream>
#include <string>
#include <unordered_map>

namespace Kintsugi
{
     // The B-tree only understands int keys, but a user thinks in "gmail", "github",
     // "the second github account". This is the translation table between the two:
     // a key like "gmail:0" gets an int id, and that id is what goes into the tree.
     //
     // Ids are handed out in order and never reused, not even after a remove. Reusing
     // one would point an old id at somebody else's entry, and there is no way to tell
     // a stale id from a fresh one.
     //
     // The whole map lives in memory and the file is only read at startup and rewritten
     // at shutdown. Fine for a few thousand keys, and it keeps every lookup a hash
     // lookup instead of a disk seek. The cost is that a crash loses whatever was
     // added since the process started.
     class Keymap
     {
          public:
               Keymap(const std::string& file_path);
               ~Keymap();

               int get_or_create(const std::string& key);
               int get(const std::string& key);    // -1 when the key is unknown

               // Careful, this matches by prefix, so counting "gmail" also counts
               // "gmail2:0". Use next_index() when you want one platform only.
               int count(const std::string& key);

               // first index that is free for "<platform>:<index>", never an index already taken
               int next_index(const std::string& platform);

               void remove(const std::string& key);

               // Copy, not a reference, so a caller cannot quietly edit the table.
               std::unordered_map<std::string, int> get_map();

          private:
               int next_id;
               std::unordered_map<std::string, int> map;
               std::fstream file_io;
               std::string file_path;
     };
}
