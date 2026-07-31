#include "../include/key_map.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace Kintsugi
{
     Keymap::Keymap(const std::string& _file_path) : file_path(_file_path)
     {
          std::ofstream create(file_path, std::ios::app);
          create.close();
          file_io.open(file_path, std::ios::in | std::ios::out);
          
          if(!file_io.is_open())
          {
               throw std::runtime_error("Could not open file: " + file_path);
          }

          file_io >> next_id;
          std::string key; int id;
          
          while(file_io >> key >> id)
          {
               map.insert({key, id});
          }
     }

     Keymap::~Keymap()
     {
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

          return -1;
     }
}
