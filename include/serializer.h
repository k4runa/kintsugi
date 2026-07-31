#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Kintsugi::Serializer
{
     struct Field
     {
          std::string key;
          std::string value;
     };

     struct Entry 
     {
          std::vector<Field> fields;
     };

     std::vector<std::uint8_t> serialize(Entry& entry);

     Entry deserialize(std::vector<std::uint8_t>& buf, std::uint32_t& offset);
}
