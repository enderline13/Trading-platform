#pragma once

#include <vector>
#include <expected>

#include "common/Types.h"
#include "common/Trade.h"
#include "common/Order.h"
#include "common/errors.h"
#include "matching/MatchingEngine.h"
#include "storage/IAccountRepository.h"
#include "storage/ITradeRepository.h"
#include "storage/IOrderRepository.h"

struct PlaceOrderCommand {
    UserId user_id = 0;
    InstrumentId instrument_id = 0;
    Order::Side side = Order::Side::BUY;
    Order::Type type = Order::Type::MARKET;
    Decimal price;
    Decimal quantity;
};

struct CancelOrderCommand {
    UserId user_id;
    OrderId order_id;
};

struct GetOrdersQuery {
    UserId user_id = 0;
    std::optional<Order::Status> status;
    std::optional<InstrumentId> instrument_id;
};

struct GetTradesQuery {
    UserId user_id;
    std::optional<InstrumentId> instrument_id;
};

class TradingCore {
public:
    TradingCore(std::shared_ptr<sql::Connection> conn, std::shared_ptr<IOrderRepository> orders,
            std::shared_ptr<ITradeRepository> trades,
            std::shared_ptr<IAccountRepository> accounts,
            std::shared_ptr<MatchingEngine> matching) : m_conn(conn), m_orders(orders), m_trades(trades), m_accounts(accounts), m_matching(matching) {}
    std::expected<OrderId, TradingError> placeOrder(const PlaceOrderCommand&);
    std::expected<void, TradingError> cancelOrder(const CancelOrderCommand&);

    std::expected<std::vector<Order>, TradingError> getUserOrders(const GetOrdersQuery&) const;
    std::expected<std::vector<Trade>, TradingError> getTradeHistory(const GetTradesQuery&) const;

private:
    std::shared_ptr<sql::Connection> m_conn;

    std::shared_ptr<IOrderRepository> m_orders;
    std::shared_ptr<ITradeRepository> m_trades;
    std::shared_ptr<IAccountRepository> m_accounts;
    std::shared_ptr<MatchingEngine> m_matching;
};