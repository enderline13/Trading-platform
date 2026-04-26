#pragma once

#include <optional>

#include "mysql/jdbc.h"

#include "common/Types.h"
#include "common/Order.h"

class IOrderRepository {
public:
    virtual ~IOrderRepository() = default;

    virtual OrderId create(const Order&) = 0;
    virtual std::optional<Order> get(OrderId) = 0;
    virtual std::vector<Order> getByUser(UserId) = 0;
    virtual void update(const Order&) = 0;
    virtual void updateStatus(OrderId id, Order::Status status, Decimal remainingQty) = 0;
};

class MySqlOrderRepository final : public IOrderRepository {
    std::shared_ptr<sql::Connection> m_conn;

public:
    explicit MySqlOrderRepository(std::shared_ptr<sql::Connection> conn) : m_conn(std::move(conn)) {}

    void updateStatus(const OrderId id, const Order::Status status, const Decimal remainingQty) override {
        PrepStatementPtr pstmt(m_conn->prepareStatement(
            "UPDATE orders SET status = ?, remaining_quantity = ? WHERE id = ?"
        ));

        pstmt->setString(1, orderStatusToString(status));
        pstmt->setString(2, decimalToSql(remainingQty));
        pstmt->setUInt64(3, id);

        if (pstmt->executeUpdate() == 0) {
            // WARN
        }
    }

    OrderId create(const Order& order) override {
        PrepStatementPtr pstmt(m_conn->prepareStatement(
            "INSERT INTO orders (user_id, instrument_id, type, side, price, quantity, remaining_quantity, status) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"
        ));
        pstmt->setUInt64(1, order.user_id);
        pstmt->setUInt64(2, order.instrument_id);
        pstmt->setString(3, orderTypeToString(order.type));
        pstmt->setString(4, orderSideToString(order.side));
        pstmt->setString(5, decimalToSql(order.price));
        pstmt->setString(6, decimalToSql(order.quantity));
        pstmt->setString(7, decimalToSql(order.quantity)); // initially remaining = total
        pstmt->setString(8, "NEW");

        pstmt->executeUpdate();

        if (ResultSetPtr res(m_conn->createStatement()->executeQuery("SELECT LAST_INSERT_ID()")); res->next()) {
            return res->getUInt64(1);
        }

        throw std::runtime_error("Failed to retrieve last insert ID for order");
    }

    std::optional<Order> get(const OrderId id) override {
        PrepStatementPtr pstmt(m_conn->prepareStatement("SELECT * FROM orders WHERE id = ?"));
        pstmt->setUInt64(1, id);

        if (ResultSetPtr res(pstmt->executeQuery()); res->next()) {
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
        PrepStatementPtr pstmt(m_conn->prepareStatement(
            "UPDATE orders SET remaining_quantity = ?, status = ? WHERE id = ?"
        ));
        pstmt->setString(1, decimalToSql(order.remaining_quantity));
        pstmt->setString(2, orderStatusToString(order.status));
        pstmt->setUInt64(3, order.id);
        pstmt->executeUpdate();
    }

    std::vector<Order> getByUser(const UserId userId) override {
        PrepStatementPtr pstmt(m_conn->prepareStatement("SELECT * FROM orders WHERE user_id = ?"));
        pstmt->setUInt64(1, userId);
        ResultSetPtr res(pstmt->executeQuery());

        std::vector<Order> orders;
        orders.reserve(res->rowsCount());

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
};