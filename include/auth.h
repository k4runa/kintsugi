#pragma once

#include <string>

namespace Kintsugi::Auth
{
     // Who is allowed to open the store. Nothing in here goes through the storage
     // engine: the users live in their own plain text file, one "username:password"
     // per line, next to the database rather than inside it.
     //
     // The passwords are written as they arrive, no hash. Anyone who can read the
     // file has every account, which is the same as saying this only keeps honest
     // people out. Hashing them is the fix, and it goes here rather than anywhere
     // above, because both functions below are the only readers of that file.
     struct UserScheme
     {
          std::string username;
          std::string password;
     };

     // Same two fields either way. They exist so a call site cannot hand a login to
     // the register path by accident, and so that whatever register grows later
     // (an email, a confirmation) has somewhere to go without touching login.
     struct RegisterScheme : UserScheme {};

     struct LoginScheme : UserScheme {};

     class AuthService
     {
          public:
               AuthService(const std::string& file_name);
               ~AuthService();

               // Both open the file themselves on every call, nothing is kept between
               // them. That is why they are const: the object holds a path and no state.
               //
               // Leading underscores only to keep clear of the `register` keyword, which
               // _register would otherwise collide with. _login is named to match it.
               bool _login(const LoginScheme& scheme) const;
               bool _register(const RegisterScheme& scheme) const;

          private:
               // Path, not a handle. The file is opened and closed per call.
               std::string file_name;
     };
}
