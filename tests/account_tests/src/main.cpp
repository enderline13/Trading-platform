#include <gtest/gtest.h>
#include "core/AccountManager.h"
#include "storage/IAccountRepository.h"

class AccountManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        accountRepo = std::make_shared<InMemoryAccountRepository>();
        accountManager = std::make_unique<AccountManager>(accountRepo);
        testUserId = 42;
    }

    std::shared_ptr<InMemoryAccountRepository> accountRepo;
    std::unique_ptr<AccountManager> accountManager;
    UserId testUserId;
};

// --- Тесты Баланса ---

TEST_F(AccountManagerTest, DepositSuccess) {
    Decimal amount{150, 500'000'000}; // 150.5
    auto result = accountManager->deposit({testUserId, amount});

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(accountManager->getBalance(testUserId).value(), amount);
}

TEST_F(AccountManagerTest, DepositNegativeAmountFails) {
    Decimal negativeAmount{-100, 0};
    auto result = accountManager->deposit({testUserId, negativeAmount});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), BalanceError::InvalidAmount);
}

TEST_F(AccountManagerTest, WithdrawSuccess) {
    // Сначала положим деньги
    accountManager->deposit({testUserId, Decimal{500, 0}});

    // Снимаем часть
    auto result = accountManager->withdraw({testUserId, Decimal{200, 0}});

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(accountManager->getBalance(testUserId).value(), Decimal(300, 0));
}

TEST_F(AccountManagerTest, WithdrawInsufficientFunds) {
    accountManager->deposit({testUserId, Decimal{100, 0}});

    auto result = accountManager->withdraw({testUserId, Decimal{150, 0}});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), BalanceError::InsuffucientMoney);
}

// --- Тесты Позиций ---

TEST_F(AccountManagerTest, GetPositionsEmptyInitially) {
    auto positions = accountManager->getPositions(testUserId).value();
    EXPECT_TRUE(positions.empty());
}

TEST_F(AccountManagerTest, GetPositionsAfterTrade) {
    // Напрямую через репозиторий имитируем результат сделки
    accountRepo->updatePosition(testUserId, 1, Decimal{10, 0}, Decimal{50, 0});

    auto positions = accountManager->getPositions(testUserId).value();
    ASSERT_EQ(positions.size(), 1);
    EXPECT_EQ(positions[0].instrument_id, 1);
    EXPECT_EQ(positions[0].quantity, Decimal(10, 0));
}