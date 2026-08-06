#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Turns an Entry into bytes and back. No schema anywhere: an entry is just a bag of
// key/value pairs, so one account can carry a "note" field and the next one does not
// have to. Keeping it schemaless means adding a field later needs no migration.
//
// Layout, all integers 4 bytes little endian:
//
//     field_count
//     for each field: key_len, key bytes, value_len, value bytes
//
// Lengths are written out instead of using a separator byte, because a password is
// allowed to contain any character we might have picked as the separator.
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

     // offset moves forward past the entry that was read, so entries packed back to
     // back in one buffer can be read in a loop. Start it at 0 for a fresh buffer.
     Entry deserialize(std::vector<std::uint8_t>& buf, std::uint32_t& offset);
}
