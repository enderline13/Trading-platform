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
    virtual void addHistoryEntry(uint64_t accountId, Decimal amount, const std::string& reason, std::optional<uint64_t> referenceId) = 0;
    virtual uint64_t getAccountIdByUserId(uint64_t userId) = 0;
    virtual std::optional<Position> getPosition(UserId userId, InstrumentId instrumentId) = 0;
    virtual void updatePosition(UserId userId, InstrumentId instrumentId, Decimal quantityDelta, Decimal price) = 0;
    virtual std::vector<Position> getPositions(UserId userId) = 0;
};

class MySqlAccountRepository : public IAccountRepository {
private:
    std::shared_ptr<sql::Connection> m_conn;

    std::string toSql(const Decimal& d) const { return d.toString(); }
    Decimal fromSql(const std::string& s) const { return decimalFromSql(s); }

public:
    uint64_t getAccountIdByUserId(uint64_t userId) {
        try {
            std::unique_ptr<sql::PreparedStatement> pstmt(m_conn->prepareStatement(
                "SELECT id FROM accounts WHERE user_id = ?"
            ));
            pstmt->setUInt64(1, userId);

            std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

            if (res->next()) {
                return res->getUInt64("id");
            } else {
                // Если аккаунта нет (чего не должно быть из-за триггера), выбрасываем исключение
                throw std::runtime_error("Account not found for user_id: " + std::to_string(userId));
            }
        } catch (sql::SQLException& e) {
            throw std::runtime_error("DB Error in getAccountIdByUserId: " + std::string(e.what()));
        }
    }

    void addHistoryEntry(
    uint64_t accountId,
    Decimal amount,
    const std::string& reason,
    std::optional<uint64_t> referenceId)
    {
        try {
            // Готовим запрос. Поля: account_id, change_amount, reason, reference_id
            std::unique_ptr<sql::PreparedStatement> pstmt(m_conn->prepareStatement(
                "INSERT INTO balance_history (account_id, change_amount, reason, reference_id) "
                "VALUES (?, ?, ?, ?)"
            ));

            // 1. ID аккаунта
            pstmt->setUInt64(1, accountId);

            // 2. Сумма изменения.
            // Преобразуем твой Decimal (units/nanos) в строку "units.nanos",
            // чтобы MySQL корректно сохранил его в DECIMAL(18,8)
            std::string amountStr = amount.toString();
            pstmt->setString(2, amountStr);

            // 3. Причина (ENUM: 'TRADE','DEPOSIT','WITHDRAWAL','FEE')
            // Важно, чтобы строка точно совпадала с определением в SQL
            pstmt->setString(3, reason);

            // 4. Ссылка на связанную сущность (например, ID сделки)
            if (referenceId.has_value()) {
                pstmt->setUInt64(4, referenceId.value());
            } else {
                // Если ссылки нет, записываем NULL
                pstmt->setNull(4, sql::DataType::BIGINT);
            }

            pstmt->executeUpdate();

        } catch (sql::SQLException& e) {
            // Здесь стоит добавить логирование, например через spdlog
            throw std::runtime_error("Failed to add balance history: " + std::string(e.what()));
        }
    }
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