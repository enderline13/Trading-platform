#include <gtest/gtest.h>
#include "core/AuthManager.h"
#include "storage/IUserRepository.h"

class AuthManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        userRepo = std::make_shared<InMemoryUserRepository>();
        authManager = std::make_unique<AuthManager>(userRepo);
    }

    std::shared_ptr<InMemoryUserRepository> userRepo;
    std::unique_ptr<AuthManager> authManager;
};

// Тест 1: Успешная регистрация
TEST_F(AuthManagerTest, RegisterSuccess) {
    RegisterCommand cmd{"dev_user", "dev@example.com", "password123"};

    auto result = authManager->registerUser(cmd);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 1); // Первый ID должен быть 1

    auto user = userRepo->getById(result.value());
    ASSERT_TRUE(user.has_value());
    EXPECT_EQ(user->username, "dev_user");
}

// Тест 2: Ошибка при регистрации дубликата Email
TEST_F(AuthManagerTest, RegisterDuplicateEmail) {
    RegisterCommand cmd{"user1", "same@email.com", "pass"};
    authManager->registerUser(cmd);

    auto result = authManager->registerUser({"user2", "same@email.com", "pass"});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthError::UserAlreadyExists);
}

// Тест 3: Успешный логин и получение JWT
TEST_F(AuthManagerTest, LoginSuccess) {
    authManager->registerUser({"trader", "trader@test.com", "secret"});

    LoginCommand loginCmd{"trader", "secret"}; // Пробуем по username
    auto tokenResult = authManager->login(loginCmd);

    ASSERT_TRUE(tokenResult.has_value());
    EXPECT_FALSE(tokenResult.value().empty());
}

// Тест 4: Валидация выпущенного токена
TEST_F(AuthManagerTest, ValidateTokenSuccess) {
    authManager->registerUser({"tester", "test@test.com", "pass"});
    auto token = authManager->login({"tester", "pass"}).value();

    auto userResult = authManager->validateToken(token);

    ASSERT_TRUE(userResult.has_value());
    EXPECT_EQ(userResult->username, "tester");
}

// Тест 5: Попытка логина с неверным паролем
TEST_F(AuthManagerTest, LoginWrongPassword) {
    authManager->registerUser({"user", "u@u.com", "real_pass"});

    auto result = authManager->login({"user", "wrong_pass"});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), AuthError::InvalidCredentials);
}