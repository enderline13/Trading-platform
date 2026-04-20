#include "core/tradingCore.h"

#include "core/TransactionGuard.h"

std::expected<OrderId, TradingError>
TradingCore::placeOrder(const PlaceOrderCommand& cmd)
{
    // 1. Инициализируем транзакцию. Если метод выйдет по return или исключению — случится rollback.
    TransactionGuard tx(m_conn);

    // Базовые валидации
    if (cmd.quantity <= Decimal(0,0))
        return std::unexpected(TradingError::InvalidOrder);

    if (cmd.type == Order::Type::LIMIT && cmd.price <= Decimal(0,0))
        return std::unexpected(TradingError::InvalidOrder);

    // Проверка баланса (только для покупки)
    // В идеале для SELL нужно проверять наличие акций в Positions, но пока сфокусируемся на деньгах.
    if (cmd.side == Order::Side::BUY) {
        Decimal userBalance = m_accounts->getBalance(cmd.user_id);
        Decimal required = cmd.price * cmd.quantity;
        if (userBalance < required)
            return std::unexpected(TradingError::InsufficientBalance);
    }

    // 2. Создаем структуру ордера
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

    // Сохраняем в БД и получаем ID
    OrderId id = m_orders->create(order);
    order.id = id;

    // 3. Отправляем в Matching Engine (работает в памяти)
    auto orderPtr = std::make_shared<Order>(order);
    auto result = m_matching->submitOrder(orderPtr);

    if (!result)
        return std::unexpected(TradingError::MatchingFailed);

    // Обновляем состояние нашего ордера после матчинга (изменится статус и remaining_quantity)
    order = *orderPtr;

    // 4. Обработка совершенных сделок
    for (const auto& trade : result->trades) {
        // Получаем данные ордеров участников, чтобы знать их user_id
        auto buyOrder = m_orders->get(trade.buy_order_id);
        auto sellOrder = m_orders->get(trade.sell_order_id);

        if (buyOrder && sellOrder) {
            // Сохраняем сделку в БД и получаем её ID для истории баланса
            uint64_t tradeId = m_trades->save(trade, buyOrder->user_id, sellOrder->user_id);

            Decimal totalCost = trade.price * trade.quantity;

            // --- ОБРАБОТКА ПОКУПАТЕЛЯ ---
            uint64_t buyerAccId = m_accounts->getAccountIdByUserId(buyOrder->user_id);
            m_accounts->changeBalance(buyOrder->user_id, -totalCost); // Уменьшаем баланс
            m_accounts->addHistoryEntry(buyerAccId, -totalCost, "TRADE", tradeId); // Пишем в историю

            m_accounts->updatePosition(buyOrder->user_id, trade.instrument_id,
                                       trade.quantity, trade.price);

            // --- ОБРАБОТКА ПРОДАВЦА ---
            uint64_t sellerAccId = m_accounts->getAccountIdByUserId(sellOrder->user_id);
            m_accounts->changeBalance(sellOrder->user_id, totalCost); // Увеличиваем баланс
            m_accounts->addHistoryEntry(sellerAccId, totalCost, "TRADE", tradeId); // Пишем в историю

            m_accounts->updatePosition(sellOrder->user_id, trade.instrument_id,
                                       -trade.quantity, trade.price);
        }
    }

    // 5. Обновляем статусы Maker-ордеров, которые были полностью или частично исполнены
    // (Их статус изменился внутри MatchingEngine, нужно синхронизировать с БД)
    for (const auto& filledId : result->filled_order_ids) {
        // Мы не можем просто создать пустой Order, нужно обновить статус в БД.
        // Предположим, у m_orders есть метод updateStatus или мы обновляем по ID
        m_orders->updateStatus(filledId, Order::Status::FILLED, Decimal(0,0));
    }

    // 6. Обновляем наш текущий (Taker) ордер
    m_orders->update(order);

    // 7. ФИНАЛЬНЫЙ КОММИТ
    // Если мы дошли до этой точки, значит ни один throw не вылетел,
    // и все изменения в БД (ордер, трейды, балансы, история) зафиксируются атомарно.
    tx.commit();

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