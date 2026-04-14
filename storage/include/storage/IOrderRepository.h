#pragma once

#include <optional>
#include <mutex>
#include <unordered_map>

#include "common/Types.h"
#include "common/Order.h"


class IOrderRepository {
public:
    virtual OrderId create(const Order&) = 0;
    virtual std::optional<Order> get(OrderId) = 0;
    virtual std::vector<Order> getByUser(UserId) = 0;
    virtual void update(const Order&) = 0;
};

class InMemoryOrderRepository : public IOrderRepository {
public:
    OrderId create(const Order& order) override {
        std::lock_guard lock(m_mutex);

        Order copy = order;
        copy.id = m_nextId++;

        m_orders[copy.id] = copy;
        m_userOrders[copy.user_id].push_back(copy.id);

        return copy.id;
    }

    std::optional<Order> get(OrderId id) override {
        std::lock_guard lock(m_mutex);

        if (!m_orders.contains(id)) return std::nullopt;
        return m_orders[id];
    }

    std::vector<Order> getByUser(UserId userId) override {
        std::lock_guard lock(m_mutex);

        std::vector<Order> result;

        for (auto id : m_userOrders[userId]) {
            result.push_back(m_orders[id]);
        }

        return result;
    }

    void update(const Order& order) override {
        std::lock_guard lock(m_mutex);

        if (m_orders.contains(order.id)) {
            m_orders[order.id] = order;
        }
    }

private:
    std::unordered_map<OrderId, Order> m_orders;
    std::unordered_map<UserId, std::vector<OrderId>> m_userOrders;

    OrderId m_nextId{1};
    std::mutex m_mutex;
};