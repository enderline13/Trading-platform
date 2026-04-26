#include "matching/orderBook.h"

std::expected<MatchResult, MatchingError>
OrderBook::processOrder(std::shared_ptr<Order> newOrder)
{
    if (!newOrder) return std::unexpected(MatchingError::EmptyOrder);
    if (newOrder->quantity <= Decimal{0,0} or newOrder->quantity != newOrder->remaining_quantity) {
        return std::unexpected(MatchingError::InvalidQuantity);
    }

    MatchResult result;
    Decimal lastTradePrice{0,0};

    if (newOrder->type == Order::Type::STOP) {
        m_stop_orders.push_back(newOrder);
        m_orders[newOrder->id] = newOrder;
        newOrder->status = Order::Status::NEW;
        result.status = Order::Status::NEW;
        return result;
    }

    auto perform_matching = [&](auto& oppQueue, auto& ownQueue) {
        while (true) {
            clean_top(oppQueue);
            if (oppQueue.empty()) break;

            auto top = oppQueue.top();

            if (newOrder->type == Order::Type::LIMIT) {
                bool can_match = (newOrder->side == Order::Side::BUY)
                    ? (newOrder->price >= top->price)
                    : (newOrder->price <= top->price);
                if (!can_match) break;
            }

            Decimal qty = std::min(newOrder->remaining_quantity, top->remaining_quantity);

            Trade trade;
            trade.quantity = qty;
            trade.price = top->price;
            trade.instrument_id = newOrder->instrument_id;
            trade.buy_order_id = (newOrder->side == Order::Side::BUY) ? newOrder->id : top->id;
            trade.sell_order_id = (newOrder->side == Order::Side::SELL) ? newOrder->id : top->id;

            result.trades.push_back(trade);

            newOrder->remaining_quantity -= qty;
            top->remaining_quantity -= qty;
            lastTradePrice = trade.price;

            if (top->remaining_quantity == Decimal{0,0}) {
                top->status = Order::Status::FILLED;
                result.filled_order_ids.push_back(top->id);
                m_orders.erase(top->id);
                oppQueue.pop();
            } else {
                top->status = Order::Status::PARTIALLY_FILLED;
                auto it = std::find_if(result.partial_fills.begin(), result.partial_fills.end(),
                          [&](const auto& pf) { return pf.order_id == top->id; });

                if (it != result.partial_fills.end()) {
                    it->remaining_qty = top->remaining_quantity;
                } else {
                    result.partial_fills.push_back({top->id, top->remaining_quantity});
                }
            }

            if (newOrder->remaining_quantity == Decimal{0,0}) {
                newOrder->status = Order::Status::FILLED;
                result.filled_order_ids.push_back(newOrder->id);
                break;
            }
        }

        if (newOrder->remaining_quantity > Decimal{0,0}) {
            if (newOrder->type == Order::Type::LIMIT) {
                newOrder->status = (newOrder->remaining_quantity < newOrder->quantity)
                    ? Order::Status::PARTIALLY_FILLED
                    : Order::Status::NEW;

                m_orders[newOrder->id] = newOrder;
                ownQueue.push(newOrder);
            } else {
                newOrder->status = (newOrder->remaining_quantity < newOrder->quantity)
                    ? Order::Status::PARTIALLY_FILLED
                    : Order::Status::CANCELED;
            }
        }
    };

    // 3. Вызов матчинга в зависимости от стороны
    if (newOrder->side == Order::Side::BUY) {
        perform_matching(m_asks, m_bids);
    } else {
        perform_matching(m_bids, m_asks);
    }

    result.status = newOrder->status;

    if (lastTradePrice > Decimal{0,0}) {
        process_stop_orders(lastTradePrice, result);
    }

    return result;
}

void OrderBook::process_stop_orders(Decimal lastPrice, MatchResult& result) {
    if (m_stop_orders.empty()) return;

    std::vector<std::shared_ptr<Order>> triggered;

    for (auto it = m_stop_orders.begin(); it != m_stop_orders.end(); ) {
        auto& o = *it;
        bool trigger = (o->side == Order::Side::BUY && lastPrice >= o->price) ||
                       (o->side == Order::Side::SELL && lastPrice <= o->price);

        if (trigger) {
            triggered.push_back(o);
            m_orders.erase(o->id);
            it = m_stop_orders.erase(it);
        } else {
            ++it;
        }
    }

    for (auto& o : triggered) {
        o->type = Order::Type::MARKET;
        o->status = Order::Status::NEW;

        auto res = processOrder(o);

        if (res) {
            result.trades.insert(result.trades.end(), res->trades.begin(), res->trades.end());
            result.filled_order_ids.insert(result.filled_order_ids.end(),
                                         res->filled_order_ids.begin(),
                                         res->filled_order_ids.end());
        }
    }
}

std::expected<void, MatchingError>
OrderBook::cancelOrder(OrderId id)
{
    auto it = m_orders.find(id);
    if (it == m_orders.end())
        return std::unexpected(MatchingError::OrderNotFound);

    if (it->second->status == Order::Status::FILLED)
        return std::unexpected(MatchingError::AlreadyFilled);

    it->second->status = Order::Status::CANCELED;
    m_orders.erase(it);

    return {};
}

std::shared_ptr<Order> OrderBook::getOrder(const OrderId id) {
    if (m_orders.contains(id)) return m_orders[id];
    return nullptr;
}