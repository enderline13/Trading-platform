#include <random>

#include "core/authManager.h"
#include "common/errors.h"

std::string generateRandomToken() {
    static const char charset[] = "0123456789ABCDEF";
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
AuthManager::registerUser(const RegisterCommand& cmd)
{
    if (cmd.username.empty() || cmd.password.empty() || cmd.email.empty())
        return std::unexpected(AuthError::InvalidInput);

    if (m_users->getByEmail(cmd.email))
        return std::unexpected(AuthError::UserAlreadyExists);

    User user;
    user.username = cmd.username;
    user.email = cmd.email;
    user.password_hash = hashPassword(cmd.password); // Твоя функция хеширования

    return m_users->create(user);
}

std::expected<Token, AuthError>
AuthManager::login(const LoginCommand& cmd)
{
    auto userOpt = m_users->getByEmail(cmd.username);
    if (!userOpt) userOpt = m_users->getByUsername(cmd.username);
    if (!userOpt) return std::unexpected(AuthError::UserNotFound);

    if (userOpt->password_hash != hashPassword(cmd.password))
        return std::unexpected(AuthError::InvalidCredentials);

    // Генерируем токен и сохраняем сессию
    std::string token = generateRandomToken();

    {
        std::unique_lock lock(m_sessionMutex);
        m_sessions[token] = userOpt->id;
    }

    return token;
}

std::expected<User, AuthError>
AuthManager::validateToken(const Token& token) const
{
    UserId uid;
    {
        std::shared_lock lock(m_sessionMutex);
        auto it = m_sessions.find(token);
        if (it == m_sessions.end()) {
            return std::unexpected(AuthError::InvalidToken);
        }
        uid = it->second;
    }

    auto userOpt = m_users->getById(uid);
    if (!userOpt) return std::unexpected(AuthError::UserNotFound);

    return *userOpt;
}

void AuthManager::logout(const Token& token) {
    std::unique_lock lock(m_sessionMutex);
    m_sessions.erase(token);
}

std::expected<User, AuthError>
AuthManager::getUser(UserId id) const
{
    auto userOpt = m_users->getById(id);
    if (!userOpt)
        return std::unexpected(AuthError::UserNotFound);

    return *userOpt;
}