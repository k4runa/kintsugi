#include "../include/serializer.h"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace Kintsugi::Serializer
{
     // The four-byte numbers are taken apart and put back together by hand instead of
     // memcpy'ing an uint32_t. Slower, but the byte order is then written down in the
     // code rather than inherited from whatever CPU built the file. Little endian:
     // lowest byte first.

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
               // Length first, then the raw bytes. No terminator and no escaping, so a
               // value is free to contain quotes, spaces, newlines, whatever.
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

          // Only the header above is bounds checked. Inside the loop we trust every
          // length we read, so a corrupt page can walk us straight off the end of buf.
          // Store hands us a whole page it read from disk, which is exactly the input
          // that could be garbage, so this wants a length check per field before it
          // ever sees a file somebody else wrote.
          for(std::uint32_t i = 0; i < field_count; ++i)
          {
               std::uint32_t key_len = (static_cast<std::uint32_t>(buf[offset + 0])) |
               (static_cast<std::uint32_t>(buf[offset + 1]) << 8) | (static_cast<std::uint32_t>(buf[offset + 2]) << 16) |
               (static_cast<std::uint32_t>(buf[offset + 3]) << 24);

               offset += 4;

               // Built from a pointer and a length, not from a C string, because the
               // bytes are not null terminated and may contain a 0 of their own.
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
