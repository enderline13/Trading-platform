#include "core/tradingCore.h"

#include <spdlog/spdlog.h>

#include "core/TransactionGuard.h"
#include "common/ProtoMapper.h"

#include "common.pb.h"

inline market::OrderBook toProto(const OrderBook& book) {
    market::OrderBook result;

    std::map<Decimal, Decimal, std::greater<Decimal>> aggregatedBids;
    auto bids = book.getBidQueue();
    while (!bids.empty()) {
        const auto& order = bids.top();
        aggregatedBids[order->price] = aggregatedBids[order->price] + order->quantity;
        bids.pop();
    }

    for (const auto& [price, qty] : aggregatedBids) {
        auto* level = result.add_bids();
        *level->mutable_price() = mapper::toProto(price);
        *level->mutable_quantity() = mapper::toProto(qty);
    }

    std::map<Decimal, Decimal, std::less<Decimal>> aggregatedAsks;
    auto asks = book.getAskQueue();
    while (!asks.empty()) {
        const auto& order = asks.top();
        aggregatedAsks[order->price] = aggregatedAsks[order->price] + order->quantity;
        asks.pop();
    }

    for (const auto& [price, qty] : aggregatedAsks) {
        auto* level = result.add_asks();
        *level->mutable_price() = mapper::toProto(price);
        *level->mutable_quantity() = mapper::toProto(qty);
    }

    return result;
}

std::expected<PlaceOrderResult, TradingError>
TradingCore::placeOrder(const PlaceOrderCommand& cmd) const
{
    spdlog::info("Placing order: UserID={}, InstID={}, Side={}, Qty={}",
                 cmd.user_id, cmd.instrument_id, static_cast<int>(cmd.side), cmd.quantity.toString());

    TransactionGuard tx(m_conn);

    auto instruments = m_instruments->getById(cmd.instrument_id);
    if (!instruments) {
        spdlog::error("Instrument {} not found", cmd.instrument_id);
        return std::unexpected(TradingError::InstrumentNotFound);
    }
    const auto& config = instruments.value();

    Decimal lockPrice = cmd.price;
    if (cmd.side == Order::Side::BUY) {
        if (cmd.type == Order::Type::MARKET) {
            auto bestAsk = m_matching->getBestAsk(cmd.instrument_id);
            if (!bestAsk) return std::unexpected(TradingError::NoLiquidity);
            lockPrice = (*bestAsk) * Decimal{1, 100000000}; // +10%
        }
        else if (cmd.type == Order::Type::STOP) {
            lockPrice = cmd.price * Decimal{1, 100000000};
            spdlog::info("STOP BUY: Locking with protection: {}", lockPrice.toString());
        }
    }

    if (cmd.quantity <= Decimal(0,0) || (cmd.quantity % config.lot_size) != Decimal(0,0)) {
        spdlog::warn("Invalid lot size for User {}: Qty={}", cmd.user_id, cmd.quantity.toString());
        return std::unexpected(TradingError::InvalidLotSize);
    }

    if (cmd.type == Order::Type::LIMIT || cmd.type == Order::Type::STOP) {
        if (cmd.price <= Decimal(0,0) || (cmd.price % config.tick_size) != Decimal(0,0)) {
            return std::unexpected(TradingError::InvalidTickSize);
        }
    }

    if (!m_accounts->isSystemRunning()) {
        spdlog::error("System not running");
        return std::unexpected(TradingError::SystemStopped);
    }

    if (cmd.quantity <= Decimal(0,0)) {
        spdlog::error("Order quantity < 0");
        return std::unexpected(TradingError::InvalidOrder);
    }
    if (cmd.type == Order::Type::LIMIT && cmd.price <= Decimal(0,0)) {
        spdlog::error("Price for limit order < 0");
        return std::unexpected(TradingError::InvalidOrder);
    }

    if (cmd.side == Order::Side::BUY) {
        Decimal required = lockPrice * cmd.quantity;
        if (!m_accounts->lockBalance(cmd.user_id, required)) {
            spdlog::warn("Insufficient balance. Required: {}", required.toString());
            return std::unexpected(TradingError::InsufficientBalance);
        }
    } else {
        if (!m_accounts->lockAsset(cmd.user_id, cmd.instrument_id, cmd.quantity)) {
            return std::unexpected(TradingError::InsufficientPosition);
        }
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
    if (!result) {
        spdlog::error("Matching failed");
        return std::unexpected(TradingError::MatchingFailed);
    }

    order = *orderPtr;

    Decimal totalSpent{0,0};

    for (const auto& trade : result->trades) {
        auto buyOrder = m_orders->get(trade.buy_order_id);
        auto sellOrder = m_orders->get(trade.sell_order_id);

        if (buyOrder && sellOrder) {
            TradeId tradeId = m_trades->save(trade, buyOrder->user_id, sellOrder->user_id);

            if (buyOrder->user_id == cmd.user_id) {
                totalSpent += (trade.price * trade.quantity);
            }

            m_accounts->settleTrade(buyOrder->user_id, sellOrder->user_id,
                                   trade.instrument_id, trade.quantity, trade.price, tradeId);

            auto trade_msg = mapper::toProto(trade);
            m_marketData->onTrade(trade.instrument_id, trade_msg);
        }
        else {
            spdlog::error("There is no both buyer and seller order in the trade");
            return std::unexpected(TradingError::MatchingFailed);
        }
    }

    broadcastOrderBook(order.instrument_id);

    auto orderFinalStatus = order.status;
    bool orderIsResting = (order.type == Order::Type::LIMIT &&
                           (orderFinalStatus == Order::Status::NEW ||
                            orderFinalStatus == Order::Status::PARTIALLY_FILLED));

    if (!orderIsResting) { // ордер полностью отработан и убран из стакана
        if (cmd.side == Order::Side::BUY) {
            Decimal totalLocked = lockPrice * cmd.quantity;
            Decimal overLocked = totalLocked - totalSpent;
            if (overLocked > Decimal{0,0}) {
                m_accounts->unlockBalance(cmd.user_id, overLocked);
                spdlog::info("Unlocked excess balance {} for OrderID={}",
                             overLocked.toString(), order.id);
            }
        } else { // SELL
            Decimal filledQty = cmd.quantity - order.remaining_quantity; // фактически исполнено
            Decimal overLocked = cmd.quantity - filledQty;
            if (overLocked > Decimal{0,0}) {
                m_accounts->unlockAsset(cmd.user_id, cmd.instrument_id, overLocked);
                spdlog::info("Unlocked excess asset {} for OrderID={}",
                             overLocked.toString(), order.id);
            }
        }
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

void TradingCore::broadcastOrderBook(const uint64_t instrumentId) const {
    const auto internalBook = m_matching->getOrderBook(instrumentId);
    if (!internalBook) {
        spdlog::error("Could not get order book for instrument {}", instrumentId);
        return;
    }

    const market::OrderBook protoBook = toProto(*internalBook.value());
    m_marketData->onBookUpdate(instrumentId, protoBook);
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
    TransactionGuard tx(m_conn);

    const auto orderOpt = m_orders->get(cmd.order_id);
    if (!orderOpt) return std::unexpected(TradingError::OrderNotFound);

    const auto& order = orderOpt.value();
    if (order.user_id != cmd.user_id) return std::unexpected(TradingError::Unauthorized);

    if (order.status == Order::Status::FILLED || order.status == Order::Status::CANCELED) {
        return std::unexpected(TradingError::InvalidOrder);
    }

    auto res = m_matching->cancelOrder(cmd.order_id);
    if (!res) {
        spdlog::error("Cancel failed in MatchingEngine for OrderID={}", cmd.order_id);
        return std::unexpected(TradingError::MatchingFailed);
    }

    if (order.side == Order::Side::BUY) {
        Decimal refundPrice = order.price;
        if (order.type == Order::Type::STOP) {
             refundPrice = order.price * Decimal{1, 100000000};
        }

        const Decimal amountToRefund = refundPrice * order.remaining_quantity;
        m_accounts->unlockBalance(order.user_id, amountToRefund);
        spdlog::info("Refunded {} USDT to User {} (Cancel Order {})",
                     amountToRefund.toString(), order.user_id, order.id);
    } else {
        m_accounts->unlockAsset(order.user_id, order.instrument_id, order.remaining_quantity);
        spdlog::info("Refunded {} units of Inst {} to User {} (Cancel Order {})",
                     order.remaining_quantity.toString(), order.instrument_id, order.user_id, order.id);
    }

    Order updated = order;
    updated.status = Order::Status::CANCELED;
    updated.remaining_quantity = Decimal{0,0};
    m_orders->update(updated);

    broadcastOrderBook(order.instrument_id);

    tx.commit();
    return {};
}

std::expected<std::vector<Order>, TradingError>
TradingCore::getUserOrders(const GetOrdersQuery& query) const
{
    spdlog::info("Getting user orders by status: {} and instrument: {}", query.status.has_value(), query.instrument_id.has_value());

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
    spdlog::info("Getting trade history by instrument: {}", query.instrument_id.has_value());
    return m_trades->getByUser(query.user_id, query.instrument_id);
}

std::optional<Decimal> TradingCore::getBestAsk(const InstrumentId instrumentId) const {
    return m_matching->getBestAsk(instrumentId);
}