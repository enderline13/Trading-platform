#pragma once

#include <expected>
#include <shared_mutex>

#include "common/Types.h"
#include "common/User.h"
#include "common/errors.h"
#include "storage/IUserRepository.h"

using Token = std::string;

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
    std::expected<User, AuthError> validateToken(const Token&) const;
    std::expected<User, AuthError> getUser(UserId) const;
    void logout(const Token& token);

private:
    std::shared_ptr<IUserRepository> m_users;
    std::shared_ptr<sql::Connection> m_conn;
    std::unordered_map<std::string, UserId> m_sessions;
    mutable std::shared_mutex m_sessionMutex;
};