#pragma once

#include "mysql/jdbc.h"

#include "common/Types.h"
#include "common/Trade.h"
#include "utils.h"

class ITradeRepository {
public:
    virtual ~ITradeRepository() = default;

    virtual uint64_t save(const Trade& trade, UserId buyer, UserId seller) = 0;
    virtual std::vector<Trade> getByUser(UserId userId, std::optional<InstrumentId> instId = std::nullopt) = 0;
};

class MySqlTradeRepository final : public ITradeRepository {
private:
    std::shared_ptr<sql::Connection> m_conn;

public:
    explicit MySqlTradeRepository(std::shared_ptr<sql::Connection> conn) : m_conn(std::move(conn)) {}

    TradeId save(const Trade& trade, const UserId buyer, const UserId seller) override {
        PrepStatementPtr pstmt(m_conn->prepareStatement(
            "INSERT INTO trades (instrument_id, buy_order_id, sell_order_id, price, quantity) VALUES (?, ?, ?, ?, ?)"
        ));
        pstmt->setUInt64(1, trade.instrument_id);
        pstmt->setUInt64(2, trade.buy_order_id);
        pstmt->setUInt64(3, trade.sell_order_id);
        pstmt->setString(4, decimalToSql(trade.price));
        pstmt->setString(5, decimalToSql(trade.quantity));

        pstmt->executeUpdate();

        StatementPtr stmt(m_conn->createStatement());

        if (ResultSetPtr res(stmt->executeQuery("SELECT LAST_INSERT_ID()")); res->next()) {
            return res->getUInt64(1);
        }
        throw std::runtime_error("Failed to retrieve last insert ID for trade");
    }

    std::vector<Trade> getByUser(const UserId userId, const std::optional<InstrumentId> instId) override {
        std::string query =
            "SELECT DISTINCT t.* FROM trades t "
            "JOIN orders o ON (t.buy_order_id = o.id OR t.sell_order_id = o.id) "
            "WHERE o.user_id = ?";

        if (instId) query += " AND t.instrument_id = " + std::to_string(instId.value());

        PrepStatementPtr pstmt(m_conn->prepareStatement(query));
        pstmt->setUInt64(1, userId);
        ResultSetPtr res(pstmt->executeQuery());

        std::vector<Trade> result;
        result.reserve(res->rowsCount());

        while (res->next()) {
            result.push_back({
                res->getUInt64("id"),
                res->getUInt64("instrument_id"),
                res->getUInt64("buy_order_id"),
                res->getUInt64("sell_order_id"),
                decimalFromSql(res->getString("price")),
                decimalFromSql(res->getString("quantity"))
            });
        }
        return result;
    }
};