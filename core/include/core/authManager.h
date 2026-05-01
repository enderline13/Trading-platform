#pragma once

#include <expected>
#include <shared_mutex>
#include <string>
#include <string_view>

#include "common/Types.h"
#include "common/User.h"
#include "common/errors.h"
#include "storage/IUserRepository.h"

struct TransparentHash {
    using is_transparent = void;

    size_t operator()(std::string_view sv) const noexcept {
        return std::hash<std::string_view>{}(sv);
    }

    size_t operator()(const std::string& s) const noexcept {
        return std::hash<std::string_view>{}(s);
    }
};

struct TransparentEqual {
    using is_transparent = void;

    bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
        return lhs == rhs;
    }
};

struct RegisterCommand {
    std::string username;
    std::string email;
    std::string password;
};

struct LoginCommand {
    std::string username;
    std::string password;
};

class AuthManager final {
public:
    AuthManager(std::shared_ptr<sql::Connection> conn, std::shared_ptr<IUserRepository> users) : m_users(std::move(users)), m_conn(std::move(conn)) {}
    std::expected<UserId, AuthError> registerUser(const RegisterCommand&) const;
    std::expected<Token, AuthError> login(const LoginCommand&);
    std::expected<User, AuthError> validateToken(Token_view) const;
    std::expected<User, AuthError> getUser(UserId) const;
    void logout(Token_view token);


private:
    std::shared_ptr<IUserRepository> m_users;
    std::shared_ptr<sql::Connection> m_conn;
    std::unordered_map<std::string, UserId, TransparentHash, TransparentEqual> m_sessions;
    mutable std::shared_mutex m_sessionMutex;
};