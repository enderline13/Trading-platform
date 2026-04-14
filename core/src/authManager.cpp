#include "core/authManager.h"

#include "common/errors.h"

#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/defaults.h>

static const std::string JWT_SECRET = "super_secret_key";

std::string hashPassword(const std::string& password) {
    return std::to_string(std::hash<std::string>{}(password));
}

std::expected<UserId, AuthError>
AuthManager::registerUser(const RegisterCommand& cmd)
{
    if (cmd.username.empty() || cmd.password.empty() || cmd.email.empty())
        return std::unexpected(AuthError::InvalidInput);

    auto existing = m_users->getByEmail(cmd.email);
    if (existing)
        return std::unexpected(AuthError::UserAlreadyExists);

    User user;
    user.username = cmd.username;
    user.email = cmd.email;
    user.password_hash = hashPassword(cmd.password);

    UserId id = m_users->create(user);

    return id;
}

std::expected<Token, AuthError>
AuthManager::login(const LoginCommand& cmd)
{
    auto userOpt = m_users->getByEmail(cmd.username);
    if (!userOpt) {
        userOpt = m_users->getByUsername(cmd.username);
    }
    if (!userOpt) return std::unexpected(AuthError::UserNotFound);

    const auto& user = *userOpt;

    if (user.password_hash != hashPassword(cmd.password))
        return std::unexpected(AuthError::InvalidCredentials);

    // Используем jwt::create<jwt::default_traits>() для явного указания трейтов
    auto token = jwt::create<jwt::traits::nlohmann_json>()
        .set_type("JWT")
        .set_issuer("trading_app")
        .set_payload_claim("user_id", jwt::basic_claim<jwt::traits::nlohmann_json>(std::to_string(user.id))) // Просто строка
        .sign(jwt::algorithm::hs256{JWT_SECRET});

    return token;
}

std::expected<User, AuthError>
AuthManager::validateToken(const Token& token) const
{
    try {
        // Явно указываем декодеру использовать дефолтные трейты (rapidjson)
        auto decoded = jwt::decode<jwt::traits::nlohmann_json>(token);

        auto verifier = jwt::verify<jwt::traits::nlohmann_json>()
            .allow_algorithm(jwt::algorithm::hs256{JWT_SECRET})
            .with_issuer("trading_app");

        verifier.verify(decoded);

        // Получаем claim как строку
        std::string userIdStr = decoded.get_payload_claim("user_id").as_string();
        UserId userId = std::stoull(userIdStr);

        auto userOpt = m_users->getById(userId);
        if (!userOpt) return std::unexpected(AuthError::UserNotFound);

        return *userOpt;
    }
    catch (const std::exception& e) {
        // Полезно для отладки: spdlog::error("JWT validation failed: {}", e.what());
        return std::unexpected(AuthError::InvalidToken);
    }
}

std::expected<User, AuthError>
AuthManager::getUser(UserId id) const
{
    auto userOpt = m_users->getById(id);
    if (!userOpt)
        return std::unexpected(AuthError::UserNotFound);

    return *userOpt;
}