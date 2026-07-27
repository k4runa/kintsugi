//include source files
#include "../include/buffer_pool.h"
#include "../include/disk_manager.h"
#include "../include/btree_node.h"

//include other libraries
#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifndef POOL_SIZE
#define POOL_SIZE 10

#endif

void print_help()
{
    std::cout <<
        "Kintsugi DB - available commands:\n"
        "  put <key> <value>     insert a key/value pair\n"
        "  get <key>              look up a key\n"
        "  update <key> <value>  update an existing key\n"
        "  del <key>              delete a key\n"
        "  range <min> <max>      list all keys in [min, max]\n"
        "  help                    show this message\n"
        "  exit / quit             leave the shell" << 
    std::endl;
}


std::vector<std::string> tokenize(const std::string& line)
{
     std::istringstream iss(line);
     std::vector<std::string> tokens;
     std::string tok;
     while(iss >> tok)
     {
          tokens.push_back(tok);
     }

     return tokens;
}


int main() 
{
    Kintsugi::Storage::DiskManager           disk_manager("database.db");
    Kintsugi::WAL::WALManager                wal_manager("logs.wal");
    Kintsugi::BufferPool::BufferPoolManager  buffer_pool(POOL_SIZE, &disk_manager, &wal_manager);
    Kintsugi::Tree::BTreeIndex               tree(&buffer_pool);

    std::cout << "Kintsugi DB shell. Type 'help' for commands, 'exit' to quit.\n";

    std::string line;
    while(true)
    {
         std::cout << "kintsugi> ";
         if(!std::getline(std::cin, line)) break;

         auto tokens = tokenize(line);
         if(tokens.empty()) continue;

         const std::string& cmd = tokens[0];
         
         try
         {
              if(cmd == "exit" || cmd == "quit")
              {
                   break;
              } 
              else if(cmd == "help")
              {
                   print_help();
              }
              else if(cmd == "put")
              {
                   if(tokens.size() != 3)
                   {
                        std::cout << "usage: put <key> <value>"  << std::endl;
                        continue;
                   }

                   int key = std::stoi(tokens[1]);
                   int value = std::stoi(tokens[2]);
                   bool ok = tree.insert(key, value);
                   std::cout << (ok ? "OK" : "ERROR: insert failed (duplicate key?)") << std::endl;
              }
              else if(cmd == "get")
              {
                   if(tokens.size() != 2)
                   {
                        std::cout << "usage: get <key>" << std::endl;
                        continue;
                   }

                   int key = std::stoi(tokens[1]);
                   int value = 0;
                   bool found = tree.search(key, &value);
                   if(found) std::cout << value << std::endl;
                   else std::cout << "not found." << std::endl;
              }
              else if(cmd == "update")
              {
                   if(tokens.size() != 3)
                   {
                        std::cout << "usage: update <key> <new_value>" << std::endl;
                        continue;
                   }
                   int key = std::stoi(tokens[1]);
                   int val = std::stoi(tokens[2]);
                   bool ok = tree.update(key, val);
                   std::cout << (ok ? "OK" : "ERROR: key not found.") << std::endl;
              }
              else if(cmd == "del")
              {
                   if(tokens.size() != 2)
                   {
                        std::cout << "usage: del <key>" << std::endl;
                        continue;
                   }

                   int key = std::stoi(tokens[1]);
                   bool ok = tree.delete_k(key);
                   std::cout << (ok ? "OK" : "ERROR: key not found.") << std::endl;
              }
              else if(cmd == "range")
              {
                   if(tokens.size() != 3)
                   {
                        std::cout << "usage: range <min> <max>" << std::endl;
                        continue;
                   }

                   std::uint32_t min_k = static_cast<std::uint32_t>(std::stoul(tokens[1]));
                   std::uint32_t max_k = static_cast<std::uint32_t>(std::stoul(tokens[2]));

                   auto results = tree.range_query(min_k, max_k);
                   if(results.empty())
                   {
                        std::cout << "no results." << std::endl;
                   }
                   else 
                   {
                        for(const auto& [k,v] : results)
                        {
                             std::cout << k << " --> " << v <<std::endl;
                        }
                   }
              }
              else 
              {
                   std::cout << "unknown command: " << cmd << "(type 'help')" << std::endl;
              }
         }
         catch (std::exception& e)
         {
              std::cerr << "ERROR: " << e.what() << std::endl;
         }
    }

    return 0;
}
