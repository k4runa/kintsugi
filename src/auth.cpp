#include "../include/auth.h"

#include <fstream>
#include <iostream>
#include <string>

namespace Kintsugi::Auth 
{
     AuthService::AuthService(const std::string& _file_name) : file_name(_file_name)
     {
          // Nothing to do
     }

     AuthService::~AuthService()
     {
          // Nothing to do
     }

     bool AuthService::_login(const LoginScheme& scheme) const
     {
          // A missing file is not an error here, the stream simply reads nothing and
          // the loop never runs. First launch, before anyone has registered, looks
          // exactly like a wrong password: false.
          std::ifstream file(file_name);
          std::string line;
          while(std::getline(file, line))
          {
               // Split on the first colon, so a colon inside the password is kept
               // whole. A line with no colon is not a record at all and is skipped,
               // the same way _register skips it, so both walks agree on what counts
               // as a user.
               auto pos = line.find(":");

               if(pos == std::string::npos) continue;

               std::string u = line.substr(0, pos);
               std::string p = line.substr(pos + 1);

               // Whole file in the worst case, one line at a time. Fine for the
               // handful of accounts this holds, and there is no index to consult
               // anyway. Comparing plain strings is also what makes the timing of
               // this leak how far a guess got, which stops mattering once the
               // passwords are hashed.
               if(u == scheme.username && p == scheme.password)
               {
                    return true;
               }
          }

          return false;
     }

     bool AuthService::_register(const RegisterScheme& scheme) const
     {
          // The colon is the separator, so a name holding one would be written as a
          // record that reads back differently than it was written: "alice:hack"
          // with any password lands as a line the loop below reads as plain "alice".
          // Refused here rather than escaped, because nothing needs a colon in a
          // name, and an escape scheme would have to be understood by both walks.
          //
          // The password is free to hold colons, everything after the first one is
          // taken as-is.
          if(scheme.username.find(':') != std::string::npos) return false;

          // Same walk as _login, only the name is compared. Reading the file to the
          // end before writing means the check and the append are two separate
          // passes: two processes registering the same name at once can both get
          // through here and both append. Single user on one machine, so it has not
          // come up, and a lock on the file is what would settle it.
          std::ifstream file(file_name);
          std::string line;
          while(std::getline(file,line))
          {
               auto pos = line.find(":");

               // A line with no colon is not a user record, so it is skipped rather
               // than parsed into a name. Without this, substr would hand back the
               // whole line as the username and a stray line could refuse a name
               // nobody had actually taken.
               if(pos == std::string::npos) continue;

               std::string u = line.substr(0, pos);

               if(u == scheme.username)
               {
                    return false;
               }
          }

          // Append, so the file keeps its earlier lines. _login returns on the first
          // match, which means the oldest line for a name is the one that counts.
          //
          // Written as it was typed. The file is the whole secret: anyone who can
          // read it has every password in it, and the database this guards is not
          // encrypted either. That is the real limit of what this class provides,
          // and no amount of parsing care changes it.
          std::ofstream f(file_name, std::ios::app);
          f << scheme.username << ":" << scheme.password << std::endl;
          return true;
     }
}
