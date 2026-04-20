#include <gtest/gtest.h>
#include "core/core.h"
#include "storage/IUserRepository.h"
#include "storage/IAccountRepository.h"
#include "matching/matchingEngine.h"
#include "storage/ITradeRepository.h"
#include "storage/IOrderRepository.h"

class CoreIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& dbMgr = DatabaseManager::instance();
        dbMgr.init("192.168.100.10:3306", "root", "root", "trading_platform");

        auto conn = dbMgr.getConnection();
        // Инициализация всех зависимостей
        auto users = std::make_shared<MySqlUserRepository>(conn);
        auto orders = std::make_shared<MySqlOrderRepository>(conn);
        auto trades = std::make_shared<MySqlTradeRepository>(conn);
        auto accounts = std::make_shared<MySqlAccountRepository>(conn);
        auto matching = std::make_shared<MatchingEngine>();

        // Создаем фасад Core
        core = std::make_unique<Core>(users, orders, trades, accounts, matching);
    }

    std::unique_ptr<Core> core;
};

TEST_F(CoreIntegrationTest, UserFinancialLifecycle) {
    // 1. Регистрация
    RegisterCommand reg{"trader34", "trader343@test.com", "secure_passs"};
    auto regRes = core->registerUser(reg);
    ASSERT_TRUE(regRes.has_value());
    UserId id = regRes.value();

    // 2. Логин и получение токена
    auto tokenRes = core->login({"trader34", "secure_passs"});
    ASSERT_TRUE(tokenRes.has_value());
    std::string token = tokenRes.value();

    // 3. Валидация токена (имитируем работу Gateway)
    auto authUser = core->validateToken(token);
    ASSERT_TRUE(authUser.has_value());
    EXPECT_EQ(authUser->id, id);

    // 4. Пополнение баланса
    Decimal depositAmt{5000, 0};
    auto depRes = core->deposit({id, depositAmt});
    EXPECT_TRUE(depRes.has_value());

    // 5. Проверка итогового баланса
    auto balance = core->getBalance(id);
    EXPECT_EQ(balance.value(), depositAmt) << balance.value().toString();
}

TEST_F(CoreIntegrationTest, FullTradeExecutionBetweenTwoUsers) {
    // Создаем Покупателя и Продавца
    UserId buyer = core->registerUser({"buyer", "bb@test.com", "p"}).value();
    UserId seller = core->registerUser({"seller", "ss@test.com", "p"}).value();

    // Покупателю даем деньги, продавцу — активы
    core->deposit({buyer, Decimal{1000, 0}});
    // Напрямую в репозиторий для теста (эмуляция внешнего депозита актива)
    // В реальности тут мог бы быть отдельный метод depositPosition
    // Но для интеграционного теста Core сойдет и так.

    InstrumentId btc = 1;

    // 1. Продавец выставляет лимитку на продажу 1 BTC за 600.0
    PlaceOrderCommand sellCmd{seller, btc, Order::Side::SELL, Order::Type::LIMIT, {600, 0}, {1, 0}};
    core->placeOrder(sellCmd);

    // 2. Покупатель выставляет лимитку на покупку 1 BTC за 600.0
    PlaceOrderCommand buyCmd{buyer, btc, Order::Side::BUY, Order::Type::LIMIT, {600, 0}, {1, 0}};
    auto buyOrderRes = core->placeOrder(buyCmd);
    ASSERT_TRUE(buyOrderRes.has_value());

    // --- ПРОВЕРКИ РЕЗУЛЬТАТОВ СДЕЛКИ ---

    // Баланс покупателя: 1000 - 600 = 400
    EXPECT_EQ(core->getBalance(buyer).value(), Decimal(400, 0));

    // Баланс продавца: 0 + 600 = 600
    EXPECT_EQ(core->getBalance(seller).value(), Decimal(600, 0));

    // Проверяем позиции (у покупателя должен появиться 1 BTC)
    auto buyerPositions = core->getPositions(buyer).value();
    ASSERT_EQ(buyerPositions.size(), 1);
    EXPECT_EQ(buyerPositions[0].quantity, Decimal(1, 0));

    // Проверяем историю трейдов (через Core)
    auto trades = core->getTradeHistory({buyer, btc});
    EXPECT_EQ(trades.value().size(), 1);
    EXPECT_EQ(trades.value()[0].price, Decimal(600, 0));
}

TEST_F(CoreIntegrationTest, OrderCancellationAndSafety) {
    UserId user = core->registerUser({"userrr", "uuu@u.com", "p"}).value();
    core->deposit({user, Decimal{1000, 0}});

    // Ставим ордер, который никто не купит (дорого)
    auto orderId = core->placeOrder({user, 1, Order::Side::BUY, Order::Type::LIMIT, {900, 0}, {1, 0}});
    ASSERT_TRUE(orderId.has_value());

    // Отменяем его
    auto cancelRes = core->cancelOrder({user, orderId.value()});
    EXPECT_TRUE(cancelRes.has_value());

    // Проверяем, что в списке активных ордеров его нет или он CANCELED
    auto orders = core->getUserOrders({user});
    ASSERT_TRUE(orders.has_value());
    bool foundAndCanceled = false;
    for (const auto& o : orders.value()) {
        if (o.id == orderId.value() && o.status == Order::Status::CANCELED) {
            foundAndCanceled = true;
        }
    }
    EXPECT_TRUE(foundAndCanceled);
}