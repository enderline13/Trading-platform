#include "matching/matchingEngine.h"

#include <mutex>

#include <spdlog/spdlog.h>

std::expected<MatchResult, MatchingError> MatchingEngine::submitOrder(const std::shared_ptr<Order> order) {
    std::scoped_lock lock(m_mutex);
    if (!order) {
        spdlog::error("MatchingEngine: Received empty order pointer");
        return std::unexpected(MatchingError::EmptyOrder);
    }

    spdlog::debug("MatchingEngine: Processing OrderID={} for InstID={}", order->id, order->instrument_id);

    auto& book = m_books[order->instrument_id];

    if (!book) {
        spdlog::info("MatchingEngine: Creating new OrderBook for InstID={}", order->instrument_id);
        book = std::make_shared<OrderBook>();
    }

    auto result = book->processOrder(order);
    if (!result) {
        spdlog::error("MatchingEngine: OrderBook processing failed for OrderID={}", order->id);
        return result;
    }

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
    spdlog::info("Matching Engine: Beginning cancel of order {}", id);
    std::scoped_lock lock(m_mutex);
    const auto it = m_order_locations.find(id);
    if (it == m_order_locations.end()) {
        spdlog::error("Order with id {} not found for cancel", id);
        return std::unexpected(MatchingError::OrderNotFound);
    }

    auto& location = it->second;
    if (auto res = location.book->cancelOrder(id); !res) return res;

    m_order_locations.erase(it);
    return {};
}

std::expected<std::shared_ptr<const OrderBook>, MatchingError>
MatchingEngine::getOrderBook(const InstrumentId id) const {
    std::scoped_lock lock(m_mutex);
    const auto it = m_books.find(id);
    if (it == m_books.end()) {
        return std::unexpected(MatchingError::InstrumentNotFound);
    }

    return it->second;
}

std::expected<std::shared_ptr<Order>, MatchingError> MatchingEngine::getOrder(const OrderId id) {
    if (!m_order_locations.contains(id)) {
        spdlog::error("Order with id {} not found for getOrder", id);
        return std::unexpected(MatchingError::OrderNotFound);
    }
    return m_order_locations[id].book->getOrder(id);
}

std::optional<Decimal> MatchingEngine::getBestAsk(const uint64_t instrumentId) {
    std::scoped_lock lock(m_mutex);
    if (!m_books.contains(instrumentId)) {
        spdlog::error("Could not find instrument with id {}", instrumentId);
        return std::nullopt;
    }
    return m_books[instrumentId]->getBestAsk();
}
