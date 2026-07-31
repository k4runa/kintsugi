#include "../include/serializer.h"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace Kintsugi::Serializer 
{
     std::vector<std::uint8_t> serialize(Entry &entry)
     {
          std::vector<std::uint8_t> buf;

          std::uint32_t field_count = entry.fields.size();
          buf.push_back((field_count) & 0xFF);
          buf.push_back((field_count >> 8) & 0xFF);
          buf.push_back((field_count >> 16) & 0xFF);
          buf.push_back((field_count >> 24) & 0xFF);

          for(auto& field : entry.fields)
          {
               std::uint32_t key_len = field.key.size();

               buf.push_back((key_len) & 0xFF);
               buf.push_back((key_len >> 8) & 0xFF);
               buf.push_back((key_len >> 16) & 0xFF);
               buf.push_back((key_len >> 24) & 0xFF);

               for(char c : field.key) buf.push_back(c);

               std::uint32_t val_len = field.value.size();

               buf.push_back((val_len) & 0xFF);
               buf.push_back((val_len >> 8) & 0xFF);
               buf.push_back((val_len >> 16) & 0xFF);
               buf.push_back((val_len >> 24) & 0xFF);

               for(char c : field.value) buf.push_back(c);
          }

          return buf;
     }

     Entry deserialize(std::vector<std::uint8_t> &buf, std::uint32_t &offset)
     {
          Entry entry;

          if(offset + 4 > buf.size())
          {
               throw std::runtime_error("Invaild data");
          }

          std::uint32_t field_count = (static_cast<std::uint32_t>(buf[offset + 0])) |
               (static_cast<std::uint32_t>(buf[offset + 1]) << 8) | (static_cast<std::uint32_t>(buf[offset + 2]) << 16) |
               (static_cast<std::uint32_t>(buf[offset + 3]) << 24);

          offset += 4;

          for(std::uint32_t i = 0; i < field_count; ++i)
          {
               std::uint32_t key_len = (static_cast<std::uint32_t>(buf[offset + 0])) |
               (static_cast<std::uint32_t>(buf[offset + 1]) << 8) | (static_cast<std::uint32_t>(buf[offset + 2]) << 16) |
               (static_cast<std::uint32_t>(buf[offset + 3]) << 24);
          
               offset += 4;

               std::string key(reinterpret_cast<const char*>(&buf[offset]), key_len);
               offset += key_len;

               std::uint32_t val_len = (static_cast<std::uint32_t>(buf[offset + 0])) |
               (static_cast<std::uint32_t>(buf[offset + 1]) << 8) | (static_cast<std::uint32_t>(buf[offset + 2]) << 16) |
               (static_cast<std::uint32_t>(buf[offset + 3]) << 24);
                    
               offset += 4;
               
               std::string value(reinterpret_cast<const char*>(&buf[offset]), val_len);
               offset += val_len;

               entry.fields.push_back({key, value});
          }

          return entry;
     }
}
