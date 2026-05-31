#pragma once

#include <cstdint>
#include <string>

struct User {
    uint64_t id = 0;
    std::string username;
    std::string email;
    std::string password_hash;
    enum class Role {
        USER,
        ADMIN
    };
    Role role = Role::USER;
    bool is_active = true;

    bool operator==(const User& other) const {
        return id == other.id;
    }
};
