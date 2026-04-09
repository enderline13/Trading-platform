#include <gtest/gtest.h>
#include "matching/matchingEngine.h"

class MatchingEngineTest : public ::testing::Test {
protected:
    MatchingEngine engine;
    const InstrumentId AAPL = 1;
    const InstrumentId TSLA = 2;
    const int64_t USER_A = 101;
    const int64_t USER_B = 102;

    std::shared_ptr<Order> createOrder(int64_t id, Order::Side side, double price, double qty, Order::Type type = Order::Type::LIMIT) {
        auto order = std::make_shared<Order>();
        order->id = id;
        order->user_id = (side == Order::Side::BUY) ? USER_A : USER_B;
        order->instrument_id = AAPL;
        order->side = side;
        order->type = type;
        order->price = Decimal::fromDouble(price);
        order->quantity = Decimal::fromDouble(qty);
        order->remaining_quantity = order->quantity;
        order->status = Order::Status::NEW;
        return order;
    }
};

TEST_F(MatchingEngineTest, LimitOrderFullMatch) {
    auto sell = createOrder(1, Order::Side::SELL, 100.0, 10.0);
    auto buy = createOrder(2, Order::Side::BUY, 100.0, 10.0);

    engine.submitOrder(sell);
    auto result = engine.submitOrder(buy);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->status, Order::Status::FILLED);
    EXPECT_EQ(result->trades.size(), 1);
    EXPECT_EQ(result->trades[0].quantity, Decimal::fromDouble(10.0));
    EXPECT_EQ(result->trades[0].price, Decimal::fromDouble(100.0));
}

TEST_F(MatchingEngineTest, LimitOrderPartialMatch) {
    auto sell = createOrder(1, Order::Side::SELL, 100.0, 50.0);
    auto buy = createOrder(2, Order::Side::BUY, 100.0, 20.0);

    engine.submitOrder(sell);
    auto result = engine.submitOrder(buy);

    EXPECT_EQ(result->status, Order::Status::FILLED);
    EXPECT_EQ(result->trades[0].quantity, Decimal::fromDouble(20.0));

    auto cancelRes = engine.cancelOrder(1);
    EXPECT_TRUE(cancelRes.has_value()); // Значит он всё еще там
}

TEST_F(MatchingEngineTest, MarketOrderWalksTheBook) {
    engine.submitOrder(createOrder(1, Order::Side::SELL, 100.0, 10.0));
    engine.submitOrder(createOrder(2, Order::Side::SELL, 105.0, 10.0));

    auto marketBuy = createOrder(3, Order::Side::BUY, 0.0, 15.0, Order::Type::MARKET);
    auto result = engine.submitOrder(marketBuy);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->trades.size(), 2);
    EXPECT_EQ(result->trades[0].price, Decimal::fromDouble(100.0));
    EXPECT_EQ(result->trades[1].price, Decimal::fromDouble(105.0));
    EXPECT_EQ(marketBuy->remaining_quantity, Decimal::fromDouble(0.0));
}

TEST_F(MatchingEngineTest, MarketOrderNoLiquidity) {
    auto marketBuy = createOrder(1, Order::Side::BUY, 0.0, 10.0, Order::Type::MARKET);
    auto result = engine.submitOrder(marketBuy);

    EXPECT_EQ(result->status, Order::Status::CANCELED);
    EXPECT_EQ(result->trades.size(), 0);
}

TEST_F(MatchingEngineTest, StopOrderTriggeredByTrade) {
    auto stopOrder = createOrder(1, Order::Side::BUY, 110.0, 5.0, Order::Type::STOP);
    engine.submitOrder(stopOrder);

    engine.submitOrder(createOrder(2, Order::Side::SELL, 100.0, 1.0));
    engine.submitOrder(createOrder(3, Order::Side::BUY, 100.0, 1.0));

    auto checkMatch = engine.submitOrder(createOrder(4, Order::Side::SELL, 110.0, 5.0));
    EXPECT_EQ(checkMatch->trades.size(), 0);

    engine.submitOrder(createOrder(5, Order::Side::SELL, 111.0, 1.0));
    auto triggerResult = engine.submitOrder(createOrder(6, Order::Side::BUY, 111.0, 1.0));

    EXPECT_GT(triggerResult->trades.size(), 1);
}

TEST_F(MatchingEngineTest, CancelNonExistentOrder) {
    auto result = engine.cancelOrder(999999);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), MatchingError::OrderNotFound);
}

TEST_F(MatchingEngineTest, ValidationInvalidQuantity) {
    auto badOrder = createOrder(1, Order::Side::BUY, 100.0, -5.0);
    auto result = engine.submitOrder(badOrder);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), MatchingError::InvalidQuantity);
}

TEST_F(MatchingEngineTest, DoubleCancellation) {
    auto order = createOrder(1, Order::Side::BUY, 100.0, 10.0);
    engine.submitOrder(order);

    EXPECT_TRUE(engine.cancelOrder(1).has_value());

    auto secondCancel = engine.cancelOrder(1);
    EXPECT_FALSE(secondCancel.has_value());
    EXPECT_EQ(secondCancel.error(), MatchingError::OrderNotFound);
}

TEST_F(MatchingEngineTest, CancelAlreadyFilledOrder) {
    engine.submitOrder(createOrder(1, Order::Side::SELL, 100.0, 10.0));
    engine.submitOrder(createOrder(2, Order::Side::BUY, 100.0, 10.0));

    auto result = engine.cancelOrder(1);
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.error() == MatchingError::OrderNotFound ||
                result.error() == MatchingError::AlreadyFilled);
}

TEST_F(MatchingEngineTest, InstrumentsIsolation) {
    auto appleSell = createOrder(1, Order::Side::SELL, 100.0, 10.0);
    appleSell->instrument_id = AAPL;

    auto tslaBuy = createOrder(2, Order::Side::BUY, 100.0, 10.0);
    tslaBuy->instrument_id = TSLA;

    engine.submitOrder(appleSell);
    auto result = engine.submitOrder(tslaBuy);

    EXPECT_EQ(result->trades.size(), 0);

    EXPECT_TRUE(engine.cancelOrder(1).has_value());
    EXPECT_TRUE(engine.cancelOrder(2).has_value());
}