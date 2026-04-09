#pragma once

#include <queue>
#include <unordered_map>
#include <memory>
#include <expected>

#include "common/Order.h"
#include "common/OrderBookLevel.h"
#include "common/errors.h"
#include "common/MatchResult.h"


class OrderBook;

struct BidComp {
    bool operator()(const std::shared_ptr<Order>& a, const std::shared_ptr<Order>& b) const {
        if (a->price != b->price) {
            return a->price < b->price;
        }
        return a->created_at > b->created_at;
    }
};

struct AskComp {
    bool operator()(const std::shared_ptr<Order>& a, const std::shared_ptr<Order>& b) const {
        if (a->price != b->price) {
            return a->price > b->price;
        }
        return a->created_at > b->created_at;
    }
};

struct OrderLocation {
    std::shared_ptr<OrderBook> book;
};

class OrderBook {
public:
    std::expected<MatchResult, MatchingError> processOrder(std::shared_ptr<Order> newOrder);
    std::expected<void, MatchingError> cancelOrder(OrderId id);

private:
    using BidQueue = std::priority_queue<std::shared_ptr<Order>, std::vector<std::shared_ptr<Order>>, BidComp>;
    using AskQueue = std::priority_queue<std::shared_ptr<Order>, std::vector<std::shared_ptr<Order>>, AskComp>;

    BidQueue m_bids{BidComp{}};
    AskQueue m_asks{AskComp{}};

    std::unordered_map<OrderId, std::shared_ptr<Order>> m_orders;
    std::vector<std::shared_ptr<Order>> m_stop_orders;

private:
    template<typename Q>
    void clean_top(Q& q) {
        while (!q.empty() && m_orders.find(q.top()->id) == m_orders.end()) {
            q.pop();
        }
    }

    void process_stop_orders(Decimal lastPrice, MatchResult& result);
};