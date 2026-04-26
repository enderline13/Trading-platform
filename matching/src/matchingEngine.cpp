#include "matching/matchingEngine.h"

#include <mutex>

std::expected<MatchResult, MatchingError> MatchingEngine::submitOrder(const std::shared_ptr<Order> order) {
    std::scoped_lock lock(m_mutex);
    if (!order) return std::unexpected(MatchingError::EmptyOrder);

    auto& book = m_books[order->instrument_id];

    if (!book)
        book = std::make_shared<OrderBook>();

    auto result = book->processOrder(order);
    if (!result) return result;

    for (auto& trade : result->trades) {
        trade.id = m_nextTradeId++;
    }

    if (order->status == Order::Status::NEW || order->status == Order::Status::PARTIALLY_FILLED) {
        m_order_locations[order->id] = OrderLocation{ .book = book };
    }

    for (const auto& filled_id : result->filled_order_ids) {
        m_order_locations.erase(filled_id);
    }

    return result;
}

std::expected<void, MatchingError>
MatchingEngine::cancelOrder(const OrderId id) {
    std::scoped_lock lock(m_mutex);
    auto it = m_order_locations.find(id);
    if (it == m_order_locations.end()) {
        return std::unexpected(MatchingError::OrderNotFound);
    }

    auto& location = it->second;

    auto res = location.book->cancelOrder(id);
    if (!res) return res;

    m_order_locations.erase(it);

    return {};
}

std::expected<std::shared_ptr<const OrderBook>, MatchingError>
MatchingEngine::getOrderBook(InstrumentId id) const {
    std::scoped_lock lock(m_mutex);
    auto it = m_books.find(id);
    if (it == m_books.end()) {
        return std::unexpected(MatchingError::InstrumentNotFound);
    }

    return it->second;
}

std::expected<std::shared_ptr<Order>, MatchingError> MatchingEngine::getOrder(OrderId id) {
    if (!m_order_locations.contains(id)) return std::unexpected(MatchingError::OrderNotFound);
    return m_order_locations[id].book->getOrder(id);
}
