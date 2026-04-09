#pragma once

#include <unordered_map>
#include <expected>
#include <atomic>

#include "common/Order.h"
#include "common/Trade.h"
#include "common/Types.h"
#include "common/errors.h"
#include "common/MatchResult.h"

#include "orderBook.h"

class MatchingEngine {
public:
    std::expected<MatchResult, MatchingError> submitOrder(const std::shared_ptr<Order> order);
    std::expected<void, MatchingError> cancelOrder(OrderId id);

    std::expected<std::shared_ptr<const OrderBook>, MatchingError> getOrderBook(InstrumentId id) const;

private:
    bool instrument_exists(InstrumentId id) const;

    std::unordered_map<OrderId, OrderLocation> m_order_locations;
    std::unordered_map<InstrumentId, std::shared_ptr<OrderBook>> m_books;

    std::atomic<uint64_t> m_nextTradeId{1};

    mutable std::mutex m_mutex;
};
