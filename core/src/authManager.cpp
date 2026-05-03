#include "core/authManager.h"

#include <random>
#include <mutex>

#include <spdlog/spdlog.h>

#include "common/errors.h"

std::string generateRandomToken() {
    static constexpr char charset[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(32);

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<int> distribution(0, 15);

    for (int i = 0; i < 32; ++i) {
        result += charset[distribution(generator)];
    }
    return result;
}

std::string hashPassword(const std::string& password) {
    return std::to_string(std::hash<std::string>{}(password));
}

std::expected<UserId, AuthError>
AuthManager::registerUser(const RegisterCommand& cmd) const
{
    spdlog::info("Registering user {}", cmd.username);

    if (cmd.username.empty() || cmd.password.empty() || cmd.email.empty()) {
        spdlog::error("Bad register info for user '{}'", cmd.username);
        return std::unexpected(AuthError::InvalidInput);
    }
    if (m_users->getByEmail(cmd.email)) {
        spdlog::error("Email already exists for user '{}'", cmd.username);
        return std::unexpected(AuthError::UserAlreadyExists);
    }
    User user;
    user.username = cmd.username;
    user.email = cmd.email;
    user.password_hash = hashPassword(cmd.password);

    return m_users->create(user);
}

std::expected<Token, AuthError>
AuthManager::login(const LoginCommand& cmd)
{
    spdlog::info("Logging in user {}", cmd.username);

    const auto userOpt = m_users->getByUsername(cmd.username);
    if (!userOpt) return std::unexpected(AuthError::UserNotFound);

    if (userOpt->password_hash != hashPassword(cmd.password))
        return std::unexpected(AuthError::InvalidCredentials);

    std::string token = generateRandomToken();
    {
        std::unique_lock lock(m_sessionMutex);
        m_sessions[token] = userOpt->id;
    }

    return token;
}

std::expected<User, AuthError>
AuthManager::validateToken(const Token_view token) const
{
    UserId uid;
    {
        std::shared_lock lock(m_sessionMutex);
        const auto it = m_sessions.find(token);
        if (it == m_sessions.end()) {
            return std::unexpected(AuthError::InvalidToken);
        }
        uid = it->second;
    }

    auto user = m_users->getById(uid);
    if (!user) return std::unexpected(AuthError::UserNotFound);

    return user.value();
}

void AuthManager::logout(const Token_view token) {
    std::unique_lock lock(m_sessionMutex);

    if (const auto it = m_sessions.find(token); it != m_sessions.end()) {
        m_sessions.erase(it);
    }
}

std::expected<User, AuthError>
AuthManager::getUser(const UserId id) const
{
    auto user = m_users->getById(id);
    if (!user)
        return std::unexpected(AuthError::UserNotFound);

    return user.value();
}