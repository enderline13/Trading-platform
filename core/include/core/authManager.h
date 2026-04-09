#pragma once

#include "common/Types.h"
#include "common/User.h"

class AuthManager {
public:
    UserId registerUser(...);
    std::string login(...); // JWT
    User validateToken(...);
};