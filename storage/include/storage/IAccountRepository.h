#pragma once

#include <mutex>
#include <unordered_map>

#include "mysql/jdbc.h"

#include "common/Types.h"
#include "common/Decimal.h"
#include "common/Position.h"
#include "DatabaseManager.h"

class IAccountRepository {
public:
    virtual ~IAccountRepository() = default;

    virtual Decimal getBalance(UserId userId) = 0;
    virtual void updateBalance(UserId userId, Decimal newBalance) = 0;
    virtual void changeBalance(UserId userId, Decimal delta) = 0;

    virtual std::optional<Position> getPosition(UserId userId, InstrumentId instrumentId) = 0;
    virtual void updatePosition(UserId userId, InstrumentId instrumentId, Decimal quantityDelta, Decimal price) = 0;
    virtual std::vector<Position> getPositions(UserId userId) = 0;
};

class InMemoryAccountRepository : public IAccountRepository {
public:
    // --- Деньги ---
    Decimal getBalance(UserId userId) override {
        std::lock_guard lock(m_mutex);
        return m_balances[userId];
    }

    void updateBalance(UserId userId, Decimal newBalance) override {
        std::lock_guard lock(m_mutex);
        m_balances[userId] = newBalance;
    }

    // --- Позиции ---
    std::optional<Position> getPosition(UserId userId, InstrumentId instrumentId) override {
        std::lock_guard lock(m_mutex);
        auto it = m_positions[userId].find(instrumentId);
        if (it == m_positions[userId].end()) return std::nullopt;
        return it->second;
    }

    void updatePosition(UserId userId, InstrumentId instrumentId, Decimal quantityDelta, Decimal price) override {
        std::lock_guard lock(m_mutex);
        auto& pos = m_positions[userId][instrumentId];

        pos.instrument_id = instrumentId;

        // Логика обновления средней цены при покупке
        if (quantityDelta > Decimal{0,0}) {
            Decimal totalCost = (pos.average_price * pos.quantity) + (price * quantityDelta);
            pos.quantity += quantityDelta;
            pos.average_price = totalCost / pos.quantity; // Убедись, что оператор / перегружен
        } else {
            // При продаже средняя цена обычно не меняется, просто уменьшается количество
            pos.quantity += quantityDelta;
        }

        // Если количество стало 0, можно либо оставить запись, либо удалить
        if (pos.quantity == Decimal{0,0}) {
            m_positions[userId].erase(instrumentId);
        }
    }

    std::vector<Position> getPositions(UserId userId) override {
        std::lock_guard lock(m_mutex);
        std::vector<Position> result;
        for (const auto& [id, pos] : m_positions[userId]) {
            result.push_back(pos);
        }
        return result;
    }

    void changeBalance(UserId userId, Decimal delta) {
        std::lock_guard lock(m_mutex);
        m_balances[userId] += delta;
    }

private:
    mutable std::mutex m_mutex;
    std::unordered_map<UserId, Decimal> m_balances;
    // Карта: UserId -> (InstrumentId -> Position)
    std::unordered_map<UserId, std::unordered_map<InstrumentId, Position>> m_positions;
};

class MySqlAccountRepository : public IAccountRepository {
private:
    std::shared_ptr<sql::Connection> m_conn;

    std::string toSql(const Decimal& d) const { return d.toString(); }
    Decimal fromSql(const std::string& s) const { return decimalFromSql(s); }

public:
    explicit MySqlAccountRepository(std::shared_ptr<sql::Connection> conn)
        : m_conn(conn) {}

    // --- Методы работы с балансом ---

    Decimal getBalance(UserId userId) override {
        auto pstmt = m_conn->prepareStatement(
            "SELECT balance_cash FROM accounts WHERE user_id = ?"
        );
        pstmt->setUInt64(1, userId);
        auto res = pstmt->executeQuery();

        if (res->next()) {
            return fromSql(res->getString(1));
        }
        return Decimal{0, 0};
    }

    void updateBalance(UserId userId, Decimal newBalance) override {
        auto pstmt = m_conn->prepareStatement(
            "UPDATE accounts SET balance_cash = ? WHERE user_id = ?"
        );
        pstmt->setString(1, toSql(newBalance));
        pstmt->setUInt64(2, userId);
        pstmt->executeUpdate();
    }

    void changeBalance(UserId userId, Decimal delta) override {
        auto pstmt = m_conn->prepareStatement(
            "UPDATE accounts SET balance_cash = balance_cash + ? WHERE user_id = ?"
        );
        pstmt->setString(1, toSql(delta));
        pstmt->setUInt64(2, userId);
        pstmt->executeUpdate();
    }

    // --- Методы работы с позициями ---

    std::optional<Position> getPosition(UserId userId, InstrumentId instrumentId) override {
        auto pstmt = m_conn->prepareStatement(
            "SELECT p.quantity, p.average_price "
            "FROM positions p "
            "JOIN accounts a ON p.account_id = a.id "
            "WHERE a.user_id = ? AND p.instrument_id = ?"
        );
        pstmt->setUInt64(1, userId);
        pstmt->setUInt64(2, instrumentId);
        auto res = pstmt->executeQuery();

        if (res->next()) {
            return Position{
                instrumentId,
                fromSql(res->getString("quantity")),
                fromSql(res->getString("average_price"))
            };
        }
        return std::nullopt;
    }

    void updatePosition(UserId userId, InstrumentId instrumentId, Decimal quantityDelta, Decimal price) override {
        // Используем ON DUPLICATE KEY UPDATE для атомарного UPSERT.
        // Формула (average_price * quantity + new_price * new_qty) / total_qty
        // применяется только при увеличении позиции (quantityDelta > 0).
        auto pstmt = m_conn->prepareStatement(
            "INSERT INTO positions (account_id, instrument_id, quantity, average_price) "
            "SELECT id, ?, ?, ? FROM accounts WHERE user_id = ? "
            "ON DUPLICATE KEY UPDATE "
            "average_price = CASE "
            "  WHEN ? > 0 THEN (average_price * quantity + ? * ?) / (quantity + ?) "
            "  ELSE average_price END, "
            "quantity = quantity + ?"
        );

        std::string qStr = toSql(quantityDelta);
        std::string pStr = toSql(price);

        pstmt->setUInt64(1, instrumentId);
        pstmt->setString(2, qStr);
        pstmt->setString(3, pStr);
        pstmt->setUInt64(4, userId);

        // Параметры для части UPDATE
        pstmt->setString(5, qStr); // для CASE WHEN > 0
        pstmt->setString(6, qStr); // new_qty
        pstmt->setString(7, pStr); // new_price
        pstmt->setString(8, qStr); // total_qty divisor
        pstmt->setString(9, qStr); // quantity = quantity + ?

        pstmt->executeUpdate();
    }

    std::vector<Position> getPositions(UserId userId) override {
        auto pstmt = m_conn->prepareStatement(
            "SELECT p.instrument_id, p.quantity, p.average_price "
            "FROM positions p "
            "JOIN accounts a ON p.account_id = a.id "
            "WHERE a.user_id = ?"
        );
        pstmt->setUInt64(1, userId);
        auto res = pstmt->executeQuery();

        std::vector<Position> positions;
        while (res->next()) {
            positions.push_back({
                res->getUInt64("instrument_id"),
                fromSql(res->getString("quantity")),
                fromSql(res->getString("average_price"))
            });
        }
        return positions;
    }
};