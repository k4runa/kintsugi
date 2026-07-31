#pragma once

#include <fstream>
#include <string>
#include <unordered_map>

namespace Kintsugi
{
     class Keymap
     {
          public:
               Keymap(const std::string& file_path);
               ~Keymap();

               int get_or_create(const std::string& key);
               int get(const std::string& key);
          
          private:
               int next_id;
               std::unordered_map<std::string, int> map;
               std::fstream file_io;
               std::string file_path;
     };
}
