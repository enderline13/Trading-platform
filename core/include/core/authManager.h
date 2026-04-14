#pragma once

#include <expected>

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

class AuthManager {
public:
    AuthManager(std::shared_ptr<IUserRepository> users) : m_users(users) {}
    std::expected<UserId, AuthError> registerUser(const RegisterCommand&);
    std::expected<Token, AuthError> login(const LoginCommand&);
    std::expected<User, AuthError> validateToken(const Token&) const;
    std::expected<User, AuthError> getUser(UserId) const;

private:
    std::shared_ptr<IUserRepository> m_users;
};