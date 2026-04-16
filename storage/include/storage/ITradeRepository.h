#pragma once

#include <mutex>
#include <unordered_map>

#include "mysql/jdbc.h"

#include "common/Types.h"
#include "common/Trade.h"

class ITradeRepository {
public:
    virtual void save(const Trade& trade, UserId buyer, UserId seller) = 0;
    virtual std::vector<Trade> getByUser(UserId userId, std::optional<InstrumentId> instId = std::nullopt) = 0;
};

class InMemoryTradeRepository : public ITradeRepository {
public:
    void save(const Trade& trade, UserId buyer, UserId seller) override {
        std::lock_guard lock(m_mutex);

        m_allTrades.push_back(trade);

        // Индексируем по переданным ID
        m_userTrades[buyer].push_back(trade);
        m_userTrades[seller].push_back(trade);
    }

    std::vector<Trade> getByUser(UserId userId, std::optional<InstrumentId> instId) override {
        std::lock_guard lock(m_mutex);

        // Если у пользователя вообще нет сделок
        if (!m_userTrades.contains(userId)) {
            return {};
        }

        const auto& userHistory = m_userTrades.at(userId);

        // Если фильтр по инструменту не задан — отдаем всю историю пользователя
        if (!instId.has_value()) {
            return userHistory;
        }

        // Если фильтр есть — фильтруем только сделки ЭТОГО пользователя (уже быстрее)
        std::vector<Trade> result;
        for (const auto& trade : userHistory) { // Исправленный цикл без лишних bindings
            if (trade.instrument_id == *instId) {
                result.push_back(trade);
            }
        }

        return result;
    }

private:
    std::vector<Trade> m_allTrades;

    // Индекс: UserId -> Список его сделок
    std::unordered_map<UserId, std::vector<Trade>> m_userTrades;

    mutable std::mutex m_mutex;
};

class MySqlTradeRepository : public ITradeRepository {
    std::shared_ptr<sql::Connection> m_conn;
public:
    MySqlTradeRepository(std::shared_ptr<sql::Connection> conn) : m_conn(conn) {}
    void save(const Trade& trade, UserId buyer, UserId seller) override {
        auto pstmt = m_conn->prepareStatement(
            "INSERT INTO trades (instrument_id, buy_order_id, sell_order_id, price, quantity) VALUES (?, ?, ?, ?, ?)"
        );
        pstmt->setInt64(1, trade.instrument_id);
        pstmt->setInt64(2, trade.buy_order_id);
        pstmt->setInt64(3, trade.sell_order_id);
        pstmt->setString(4, decimalToSql(trade.price));
        pstmt->setString(5, decimalToSql(trade.quantity));
        pstmt->executeUpdate();
    }

    std::vector<Trade> getByUser(UserId userId, std::optional<InstrumentId> instId) override {
        std::string query =
            "SELECT DISTINCT t.* FROM trades t "
            "JOIN orders o ON (t.buy_order_id = o.id OR t.sell_order_id = o.id) "
            "WHERE o.user_id = ?";

        if (instId) query += " AND t.instrument_id = " + std::to_string(*instId);

        auto pstmt = m_conn->prepareStatement(query);
        pstmt->setInt64(1, userId);
        auto res = pstmt->executeQuery();

        std::vector<Trade> result;
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