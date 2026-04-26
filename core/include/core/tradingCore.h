#pragma once

#include <vector>
#include <expected>

#include "common/Types.h"
#include "common/Trade.h"
#include "common/Order.h"
#include "common/Instrument.h"
#include "common/errors.h"
#include "matching/MatchingEngine.h"
#include "storage/IAccountRepository.h"
#include "storage/ITradeRepository.h"
#include "storage/IOrderRepository.h"
#include "storage/IInstrumentRepository.h"

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
    std::optional<Order::Status> status = std::nullopt;
    std::optional<InstrumentId> instrument_id = std::nullopt;
};

struct GetTradesQuery {
    UserId user_id;
    std::optional<InstrumentId> instrument_id = std::nullopt;
};

class TradingCore final {
public:
    TradingCore(std::shared_ptr<sql::Connection> conn, std::shared_ptr<IOrderRepository> orders,
            std::shared_ptr<ITradeRepository> trades,
            std::shared_ptr<IAccountRepository> accounts, std::shared_ptr<IInstrumentRepository> instruments,
            std::shared_ptr<MatchingEngine> matching) : m_conn(std::move(conn)), m_orders(std::move(orders)), m_trades(std::move(trades)), m_accounts(std::move(accounts)), m_matching(std::move(matching)), m_instruments(std::move(instruments)) {}

    std::expected<OrderId, TradingError> placeOrder(const PlaceOrderCommand&) const;
    std::expected<void, TradingError> cancelOrder(const CancelOrderCommand&) const;
    std::expected<std::vector<Order>, TradingError> getUserOrders(const GetOrdersQuery&) const;
    std::expected<std::vector<Trade>, TradingError> getTradeHistory(const GetTradesQuery&) const;
    std::vector<Instrument> getAllInstruments() const;
    std::expected<Order, TradingError> getOrder(OrderId orderId) const;

private:
    std::shared_ptr<sql::Connection> m_conn;
    std::shared_ptr<IOrderRepository> m_orders;
    std::shared_ptr<ITradeRepository> m_trades;
    std::shared_ptr<IAccountRepository> m_accounts;
    std::shared_ptr<MatchingEngine> m_matching;
    std::shared_ptr<IInstrumentRepository> m_instruments;
};