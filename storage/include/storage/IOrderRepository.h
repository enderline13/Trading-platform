#pragma once

#include <optional>
#include <mutex>
#include <unordered_map>

#include "mysql/jdbc.h"

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

class MySqlOrderRepository : public IOrderRepository {
    std::shared_ptr<sql::Connection> m_conn;
public:
    MySqlOrderRepository(std::shared_ptr<sql::Connection> conn) : m_conn(conn) {}

    OrderId create(const Order& order) override {
        auto pstmt = m_conn->prepareStatement(
            "INSERT INTO orders (user_id, instrument_id, type, side, price, quantity, remaining_quantity, status) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"
        );
        pstmt->setUInt64(1, order.user_id);
        pstmt->setUInt64(2, order.instrument_id);
        pstmt->setString(3, orderTypeToString(order.type));
        pstmt->setString(4, orderSideToString(order.side));
        pstmt->setString(5, decimalToSql(order.price));
        pstmt->setString(6, decimalToSql(order.quantity));
        pstmt->setString(7, decimalToSql(order.quantity)); // initially remaining = total
        pstmt->setString(8, "NEW");

        pstmt->executeUpdate();

        auto res = m_conn->createStatement()->executeQuery("SELECT LAST_INSERT_ID()");
        res->next();
        return res->getUInt64(1);
    }

    std::optional<Order> get(OrderId id) override {
        auto pstmt = m_conn->prepareStatement("SELECT * FROM orders WHERE id = ?");
        pstmt->setUInt64(1, id);
        auto res = pstmt->executeQuery();

        if (res->next()) {
            return Order{
                res->getUInt64("id"),
                res->getUInt64("user_id"),
                res->getUInt64("instrument_id"),
                stringToOrderType(res->getString("type")),
                stringToOrderSide(res->getString("side")),
                decimalFromSql(res->getString("price")),
                decimalFromSql(res->getString("quantity")),
                decimalFromSql(res->getString("remaining_quantity")),
                stringToOrderStatus(res->getString("status"))
            };
        }
        return std::nullopt;
    }

    void update(const Order& order) override {
        auto pstmt = m_conn->prepareStatement(
            "UPDATE orders SET remaining_quantity = ?, status = ? WHERE id = ?"
        );
        pstmt->setString(1, decimalToSql(order.remaining_quantity));
        pstmt->setString(2, orderStatusToString(order.status));
        pstmt->setUInt64(3, order.id);
        pstmt->executeUpdate();
    }

    std::vector<Order> getByUser(UserId userId) override {
        auto pstmt = m_conn->prepareStatement("SELECT * FROM orders WHERE user_id = ?");
        pstmt->setUInt64(1, userId);
        auto res = pstmt->executeQuery();

        std::vector<Order> orders;
        while (res->next()) {
            orders.push_back(Order{
                res->getUInt64("id"),
                res->getUInt64("user_id"),
                res->getUInt64("instrument_id"),
                stringToOrderType(res->getString("type")),
                stringToOrderSide(res->getString("side")),
                decimalFromSql(res->getString("price")),
                decimalFromSql(res->getString("quantity")),
                decimalFromSql(res->getString("remaining_quantity")),
                stringToOrderStatus(res->getString("status"))
            });
        }
        return orders;
    }

    // getByUser реализуется аналогично через SELECT
};