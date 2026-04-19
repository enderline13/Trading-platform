#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "core/tradingCore.h"
#include "storage/IUserRepository.h"
#include "storage/IOrderRepository.h"
#include "storage/IAccountRepository.h"
#include "storage/ITradeRepository.h"


class TradingCoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& dbMgr = DatabaseManager::instance();
        dbMgr.init("192.168.100.10:3306", "root", "root", "trading_platform");

        auto conn = dbMgr.getConnection();
        // Инициализируем репозитории и движок
        userRepo = std::make_shared<MySqlUserRepository>(conn);
        orderRepo = std::make_shared<MySqlOrderRepository>(conn);
        tradeRepo = std::make_shared<MySqlTradeRepository>(conn);
        accountRepo = std::make_shared<MySqlAccountRepository>(conn);
        matchingEngine = std::make_shared<MatchingEngine>();

        tradingCore = std::make_unique<TradingCore>(
            orderRepo, tradeRepo, accountRepo, matchingEngine
        );

        // Подготовим тестового пользователя с балансом
        testUserId = 11;
        accountRepo->updateBalance(testUserId, Decimal{1000, 0});
    }

    std::shared_ptr<IUserRepository> userRepo;
    std::shared_ptr<IOrderRepository> orderRepo;
    std::shared_ptr<ITradeRepository> tradeRepo;
    std::shared_ptr<IAccountRepository> accountRepo;
    std::shared_ptr<MatchingEngine> matchingEngine;
    std::unique_ptr<TradingCore> tradingCore;
    UserId testUserId;
};

// --- ТЕСТ-КЕЙСЫ ---

// 1. Успешное размещение лимитного ордера (без немедленного исполнения)
TEST_F(TradingCoreTest, PlaceLimitOrderSuccess) {
    PlaceOrderCommand cmd{
        .user_id = testUserId,
        .instrument_id = 1,
        .side = Order::Side::BUY,
        .type = Order::Type::LIMIT,
        .price = Decimal{100, 0},
        .quantity = Decimal{5, 0}
    };

    auto result = tradingCore->placeOrder(cmd);

    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result.value(), 0);

    auto order = orderRepo->get(result.value());
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->status, Order::Status::NEW);
}

// 2. Ошибка: Недостаточно средств для покупки
TEST_F(TradingCoreTest, InsufficientBalanceForBuy) {
    PlaceOrderCommand cmd{
        .user_id = testUserId,
        .instrument_id = 1,
        .side = Order::Side::BUY,
        .type = Order::Type::LIMIT,
        .price = Decimal{1000, 0},
        .quantity = Decimal{2, 0} // Нужно 2000, а у нас 1000
    };

    auto result = tradingCore->placeOrder(cmd);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TradingError::InsufficientBalance);
}

// 3. Исполнение сделки (Matching): Buy Limit встречает Sell Limit
TEST_F(TradingCoreTest, FullTradeExecution) {
    UserId sellerId = 12;
    accountRepo->updatePosition(sellerId, 1, Decimal{10, 0}, Decimal{50, 0}); // Даем продавцу активы

    // 1. Выставляем ордер на продажу
    tradingCore->placeOrder({
        .user_id = sellerId,
        .instrument_id = 1,
        .side = Order::Side::SELL,
        .type = Order::Type::LIMIT,
        .price = Decimal{50, 0},
        .quantity = Decimal{2, 0}
    });

    // 2. Выставляем ордер на покупку по той же цене
    auto buyResult = tradingCore->placeOrder({
        .user_id = testUserId,
        .instrument_id = 1,
        .side = Order::Side::BUY,
        .type = Order::Type::LIMIT,
        .price = Decimal{50, 0},
        .quantity = Decimal{2, 0}
    });

    // Проверяем статус ордера покупателя
    auto buyOrder = orderRepo->get(buyResult.value());
    EXPECT_EQ(buyOrder->status, Order::Status::FILLED) << orderStatusToString(buyOrder->status) << buyOrder.value().id;

    // Проверяем балансы (1000 - 50*2 = 900)
    EXPECT_EQ(accountRepo->getBalance(testUserId), Decimal(900, 0)) << "value: " << accountRepo->getBalance(testUserId).toString();

    // Проверяем, что трейд записан
    auto trades = tradeRepo->getByUser(testUserId, std::nullopt);
    EXPECT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].quantity, Decimal(2, 0));
}

// 4. Отмена ордера
TEST_F(TradingCoreTest, CancelOrderSuccess) {
    auto orderId = tradingCore->placeOrder({
        .user_id = testUserId,
        .instrument_id = 1,
        .side = Order::Side::BUY,
        .type = Order::Type::LIMIT,
        .price = Decimal{10, 0},
        .quantity = Decimal{1, 0}
    }).value();

    auto cancelRes = tradingCore->cancelOrder({testUserId, orderId});

    EXPECT_TRUE(cancelRes.has_value());
    auto order = orderRepo->get(orderId);
    EXPECT_EQ(order->status, Order::Status::CANCELED);
}

// 5. Попытка отменить чужой ордер
TEST_F(TradingCoreTest, UnauthorizedCancelAttempt) {
    auto orderId = tradingCore->placeOrder({
        .user_id = testUserId,
        .instrument_id = 1,
        .side = Order::Side::BUY,
        .type = Order::Type::LIMIT,
        .price = Decimal{10, 0},
        .quantity = Decimal{1, 0}
    }).value();

    auto cancelRes = tradingCore->cancelOrder({999, orderId}); // Другой user_id

    ASSERT_FALSE(cancelRes.has_value());
    EXPECT_EQ(cancelRes.error(), TradingError::Unauthorized);
}