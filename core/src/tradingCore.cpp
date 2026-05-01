#include "core/tradingCore.h"

#include "core/TransactionGuard.h"

std::expected<PlaceOrderResult, TradingError>
TradingCore::placeOrder(const PlaceOrderCommand& cmd) const
{
    TransactionGuard tx(m_conn);

    auto instruments = m_instruments->getById(cmd.instrument_id);
    if (!instruments) return std::unexpected(TradingError::InstrumentNotFound);
    const auto& config = instruments.value();

    if (cmd.quantity <= Decimal(0,0) || (cmd.quantity % config.lot_size) != Decimal(0,0)) {
        return std::unexpected(TradingError::InvalidLotSize);
    }

    if (cmd.type == Order::Type::LIMIT) {
        if (cmd.price <= Decimal(0,0) || (cmd.price % config.tick_size) != Decimal(0,0)) {
            return std::unexpected(TradingError::InvalidTickSize);
        }
    }

    if (!m_accounts->isSystemRunning()) return std::unexpected(TradingError::SystemStopped);
    if (cmd.quantity <= Decimal(0,0)) return std::unexpected(TradingError::InvalidOrder);
    if (cmd.type == Order::Type::LIMIT && cmd.price <= Decimal(0,0)) return std::unexpected(TradingError::InvalidOrder);

    if (cmd.side == Order::Side::BUY) {
        auto userBalance = m_accounts->getBalance(cmd.user_id);
        if (!userBalance) return std::unexpected(TradingError::UserNotFound);
        if (Decimal required = cmd.price * cmd.quantity; userBalance.value() < required)
            return std::unexpected(TradingError::InsufficientBalance);
    }

    if (cmd.side == Order::Side::SELL) {
        auto position = m_accounts->getPosition(cmd.user_id, cmd.instrument_id);
        if (!position) return std::unexpected(TradingError::InsufficientPosition);
        if (position.value().quantity < cmd.quantity) return std::unexpected(TradingError::InsufficientPosition);
    }

    Order order;
    order.user_id = cmd.user_id;
    order.instrument_id = cmd.instrument_id;
    order.side = cmd.side;
    order.type = cmd.type;
    order.price = cmd.price;
    order.quantity = cmd.quantity;
    order.remaining_quantity = cmd.quantity;
    order.status = Order::Status::NEW;
    order.created_at = std::chrono::system_clock::now();

    OrderId id = m_orders->create(order);
    order.id = id;

    auto orderPtr = std::make_shared<Order>(order);
    auto result = m_matching->submitOrder(orderPtr);
    if (!result)
        return std::unexpected(TradingError::MatchingFailed);

    order = *orderPtr;

    for (const auto& trade : result->trades) {
        auto buyOrder = m_orders->get(trade.buy_order_id);
        auto sellOrder = m_orders->get(trade.sell_order_id);

        if (buyOrder && sellOrder) {
            TradeId tradeId = m_trades->save(trade, buyOrder->user_id, sellOrder->user_id);
            Decimal totalCost = trade.price * trade.quantity;

            AccountId buyerAccId = m_accounts->getAccountIdByUserId(buyOrder->user_id);
            m_accounts->changeBalance(buyOrder->user_id, -totalCost);
            m_accounts->addHistoryEntry(buyerAccId, -totalCost, "TRADE", tradeId);
            m_accounts->updatePosition(buyOrder->user_id, trade.instrument_id,
                                       trade.quantity, trade.price);

            AccountId sellerAccId = m_accounts->getAccountIdByUserId(sellOrder->user_id);
            m_accounts->changeBalance(sellOrder->user_id, totalCost);
            m_accounts->addHistoryEntry(sellerAccId, totalCost, "TRADE", tradeId);
            m_accounts->updatePosition(sellOrder->user_id, trade.instrument_id,
                                       -trade.quantity, trade.price);
        }
        else return std::unexpected(TradingError::MatchingFailed);
    }

    for (const auto& filledId : result->filled_order_ids) {
        if (filledId != order.id) {
            m_orders->updateStatus(filledId, Order::Status::FILLED, Decimal(0,0));
        }
    }
    for (const auto& pf : result->partial_fills) {
        if (pf.order_id != order.id) {
            m_orders->updateStatus(pf.order_id, Order::Status::PARTIALLY_FILLED, pf.remaining_qty);
        }
    }
    m_orders->update(order);

    tx.commit();
    return PlaceOrderResult{id, order.status};
}

std::vector<Instrument> TradingCore::getAllInstruments() const {
    return m_instruments->getAll();
}

std::expected<Order, TradingError> TradingCore::getOrder(const OrderId orderId) const {
    auto order = m_orders->get(orderId);
    if (!order) {
        return std::unexpected(TradingError::OrderNotFound);
    }

    return order.value();
}

std::expected<void, TradingError>
TradingCore::cancelOrder(const CancelOrderCommand& cmd) const
{
    auto orderOpt = m_orders->get(cmd.order_id);
    if (!orderOpt)
        return std::unexpected(TradingError::OrderNotFound);
    const auto& order = orderOpt.value();
    if (order.user_id != cmd.user_id)
        return std::unexpected(TradingError::Unauthorized);

    auto res = m_matching->cancelOrder(cmd.order_id);
    if (!res) return std::unexpected(TradingError::MatchingFailed);

    Order updated = order;
    updated.status = Order::Status::CANCELED;
    m_orders->update(updated);

    return {};
}

std::expected<std::vector<Order>, TradingError>
TradingCore::getUserOrders(const GetOrdersQuery& query) const
{
    auto orders = m_orders->getByUser(query.user_id);

    std::vector<Order> result;

    auto filtered = orders
        | std::views::filter([&](const Order& o) {
            if (query.status && o.status != query.status.value())
                return false;
            if (query.instrument_id && o.instrument_id != query.instrument_id.value())
                return false;
            return true;
        });

    std::ranges::copy(filtered, std::back_inserter(result));

    return result;
}

std::expected<std::vector<Trade>, TradingError>
TradingCore::getTradeHistory(const GetTradesQuery& query) const
{
    return m_trades->getByUser(query.user_id, query.instrument_id);
}