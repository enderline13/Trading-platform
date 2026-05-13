#include "matching/orderBook.h"

#include <spdlog/spdlog.h>

std::expected<MatchResult, MatchingError>
OrderBook::processOrder(std::shared_ptr<Order> newOrder)
{
    if (!newOrder) return std::unexpected(MatchingError::EmptyOrder);
    if (newOrder->quantity <= Decimal{0,0} or newOrder->quantity != newOrder->remaining_quantity) {
        spdlog::error("OrderBook: Invalid quantity for OrderID={}", newOrder->id);
        return std::unexpected(MatchingError::InvalidQuantity);
    }

    MatchResult result;
    Decimal lastTradePrice{0,0};

    if (newOrder->type == Order::Type::STOP) {
        spdlog::info("OrderBook: STOP order registered: ID={}", newOrder->id);
        m_stop_orders.push_back(newOrder);
        m_orders[newOrder->id] = newOrder;
        newOrder->status = Order::Status::NEW;
        result.status = Order::Status::NEW;
        return result;
    }

    auto perform_matching = [&](auto& oppQueue, auto& ownQueue) {
        spdlog::debug("OrderBook: Starting matching for OrderID={}, Side={}",
                      newOrder->id, static_cast<int>(newOrder->side));

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

            spdlog::info("OrderBook: MATCH FOUND: Order {} with Order {}. Qty={}, Price={}",
                         newOrder->id, top->id, qty.toString(), top->price.toString());

            Trade trade;
            trade.quantity = qty;
            trade.price = top->price;
            trade.instrument_id = newOrder->instrument_id;
            trade.buy_order_id = (newOrder->side == Order::Side::BUY) ? newOrder->id : top->id;
            trade.sell_order_id = (newOrder->side == Order::Side::SELL) ? newOrder->id : top->id;
            trade.executed_at = std::chrono::system_clock::now();

            result.trades.push_back(trade);

            newOrder->remaining_quantity -= qty;
            top->remaining_quantity -= qty;
            lastTradePrice = trade.price;

            if (top->remaining_quantity == Decimal{0,0}) {
                top->status = Order::Status::FILLED;
                result.filled_order_ids.push_back(top->id);
                m_orders.erase(top->id);
                oppQueue.pop();
                spdlog::debug("OrderBook: Maker Order {} fully filled", top->id);
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
                spdlog::debug("OrderBook: Taker Order {} fully filled", newOrder->id);
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
                spdlog::debug("OrderBook: Order {} added to the book", newOrder->id);
            } else {
                newOrder->status = (newOrder->remaining_quantity < newOrder->quantity)
                    ? Order::Status::PARTIALLY_FILLED
                    : Order::Status::CANCELED;
                spdlog::info("OrderBook: Market order {} partial/full cancel due to no liquidity", newOrder->id);
            }
        }
    };

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

void OrderBook::process_stop_orders(const Decimal lastPrice, MatchResult& result) {
    if (m_stop_orders.empty()) return;

    std::vector<std::shared_ptr<Order>> triggered;

    for (auto it = m_stop_orders.begin(); it != m_stop_orders.end(); ) {
        auto& o = *it;
        const bool trigger = (o->side == Order::Side::BUY && lastPrice >= o->price) ||
                       (o->side == Order::Side::SELL && lastPrice <= o->price);

        if (trigger) {
            spdlog::info("OrderBook: STOP Order {} triggered at price {}", o->id, lastPrice.toString());
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

        if (auto res = processOrder(o); res) {
            result.trades.insert(result.trades.end(), res->trades.begin(), res->trades.end());
            result.filled_order_ids.insert(result.filled_order_ids.end(),
                                         res->filled_order_ids.begin(),
                                         res->filled_order_ids.end());

            result.partial_fills.insert(result.partial_fills.end(),
                                      res->partial_fills.begin(),
                                      res->partial_fills.end());
        }
    }
}

std::expected<void, MatchingError>
OrderBook::cancelOrder(OrderId id)
{
    auto it = m_orders.find(id);
    if (it == m_orders.end()) {
        spdlog::error("OrderBook: Order with id {} not found for cancel", id);
        return std::unexpected(MatchingError::OrderNotFound);
    }
    if (it->second->status == Order::Status::FILLED) {
        spdlog::error("OrderBook: cannot cancel order because it's already filled", id);
        return std::unexpected(MatchingError::AlreadyFilled);
    }
    it->second->status = Order::Status::CANCELED;
    it->second->remaining_quantity = Decimal{0,0};

    m_orders.erase(it);
    clean_top(m_bids);
    clean_top(m_asks);

    spdlog::info("OrderBook: Order {} marked as CANCELED", id);

    return {};
}

std::shared_ptr<Order> OrderBook::getOrder(const OrderId id) {
    if (m_orders.contains(id)) return m_orders[id];
    return nullptr;
}

std::optional<Decimal> OrderBook::getBestAsk() const {
    if (m_asks.empty()) return std::nullopt;
    return m_asks.top()->price;
}

std::optional<Decimal> OrderBook::getBestBid() const {
    if (m_bids.empty()) return std::nullopt;
    return m_bids.top()->price;
}