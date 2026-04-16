#include "core/tradingCore.h"

std::expected<OrderId, TradingError>
TradingCore::placeOrder(const PlaceOrderCommand& cmd)
{
    if (cmd.quantity <= Decimal(0,0))
        return std::unexpected(TradingError::InvalidOrder);

    if (cmd.type == Order::Type::LIMIT && cmd.price <= Decimal(0,0))
        return std::unexpected(TradingError::InvalidOrder);

    Decimal userBalance = m_accounts->getBalance(cmd.user_id);

    if (cmd.side == Order::Side::BUY) {
        Decimal required = cmd.price * cmd.quantity;
        if (userBalance < required)
            return std::unexpected(TradingError::InsufficientBalance);
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
        // 1. Получаем ID пользователей (твой выбранный вариант через ордера)
        auto buyOrder = m_orders->get(trade.buy_order_id);
        auto sellOrder = m_orders->get(trade.sell_order_id);

        if (buyOrder && sellOrder) {
            // 2. Сохраняем трейд (с нашей новой сигнатурой save)
            m_trades->save(trade, buyOrder->user_id, sellOrder->user_id);

            // 3. Обновляем деньги
            Decimal totalAmount = trade.price * trade.quantity;
            m_accounts->changeBalance(buyOrder->user_id, -totalAmount);
            m_accounts->changeBalance(sellOrder->user_id, totalAmount);

            // 4. ОБНОВЛЯЕМ ПОЗИЦИИ (Вот здесь скорее всего пусто!)
            m_accounts->updatePosition(buyOrder->user_id, trade.instrument_id,
                                       trade.quantity, trade.price);
            m_accounts->updatePosition(sellOrder->user_id, trade.instrument_id,
                                       -trade.quantity, trade.price);
        }
    }

    for (const auto& id : result->filled_order_ids) {
        m_orders->update(Order{.id = id, .remaining_quantity = 0, .status = OrderStatus::FILLED});
    }

    m_orders->update(order);

    return id;
}

std::expected<void, TradingError>
TradingCore::cancelOrder(const CancelOrderCommand& cmd)
{
    auto orderOpt = m_orders->get(cmd.order_id);
    if (!orderOpt)
        return std::unexpected(TradingError::OrderNotFound);

    const auto& order = *orderOpt;

    if (order.user_id != cmd.user_id)
        return std::unexpected(TradingError::Unauthorized);

    auto res = m_matching->cancelOrder(cmd.order_id);
    if (!res)
        return std::unexpected(TradingError::MatchingFailed);

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

    for (const auto& o : orders) {
        if (query.status && o.status != *query.status)
            continue;

        if (query.instrument_id && o.instrument_id != *query.instrument_id)
            continue;

        result.push_back(o);
    }

    return result;
}

std::expected<std::vector<Trade>, TradingError>
TradingCore::getTradeHistory(const GetTradesQuery& query) const
{
    auto trades = m_trades->getByUser(query.user_id, query.instrument_id);
    return trades;
}