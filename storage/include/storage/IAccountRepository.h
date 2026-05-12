#pragma once

#include "mysql/jdbc.h"

#include "common/Types.h"
#include "common/Decimal.h"
#include "common/Position.h"
#include "DatabaseManager.h"
#include "utils.h"

struct BalanceHistoryEntry {
    Decimal change_amount;
    std::string reason;
    std::optional<uint64_t> reference_id;
    std::chrono::system_clock::time_point timestamp;
};

class IAccountRepository {
public:
    virtual ~IAccountRepository() = default;

    virtual std::optional<Decimal> getBalance(UserId userId) = 0;
    virtual void updateBalance(UserId userId, Decimal newBalance) = 0;
    virtual void changeBalance(UserId userId, Decimal delta) = 0;
    virtual std::optional<Position> getPosition(UserId userId, InstrumentId instrumentId) = 0;
    virtual void updatePosition( UserId userId, InstrumentId instrumentId, Decimal quantityDelta, Decimal price) = 0;
    virtual std::vector<Position> getPositions( UserId userId) = 0;
    virtual void addPosition(uint64_t userId, uint64_t instId, Decimal qty) = 0;
    virtual void addHistoryEntry(AccountId accountId, Decimal amount, const std::string& reason, std::optional<uint64_t> referenceId) = 0;
    virtual AccountId getAccountIdByUserId(uint64_t userId) = 0;
    virtual std::vector<BalanceHistoryEntry> getHistory(uint64_t accountId) = 0;
    virtual void setSystemStatus(bool running) = 0;
    virtual bool isSystemRunning() = 0;

    virtual bool lockBalance(UserId userId, Decimal amount) = 0;
    virtual void unlockBalance(UserId userId, Decimal amount) = 0;

    virtual bool lockAsset(UserId userId, InstrumentId instId, Decimal qty) = 0;
    virtual void unlockAsset(UserId userId, InstrumentId instId, Decimal qty) = 0;

    virtual void settleTrade(UserId buyerId, UserId sellerId, InstrumentId instId, Decimal qty, Decimal price, TradeId tradeId) = 0;
};

class MySqlAccountRepository final : public IAccountRepository {
private:
    std::shared_ptr<sql::Connection> m_conn;

    void updateBalanceReserved(const UserId userId, const Decimal delta) const {
        PrepStatementPtr pstmt(m_conn->prepareStatement(
            "UPDATE accounts SET balance_reserved = balance_reserved + ? WHERE user_id = ?"
        ));

        pstmt->setString(1, delta.toString());
        pstmt->setUInt64(2, userId);

        pstmt->executeUpdate();
    }

    void updateAssetReserved(const UserId userId, const InstrumentId instId, const Decimal delta) const {
        PrepStatementPtr pstmt(m_conn->prepareStatement(
            "UPDATE positions SET quantity_reserved = quantity_reserved + ? "
            "WHERE instrument_id = ? AND account_id = (SELECT id FROM accounts WHERE user_id = ?)"
        ));
        pstmt->setString(1, delta.toString());
        pstmt->setUInt64(2, instId);
        pstmt->setUInt64(3, userId);
        pstmt->executeUpdate();
    }

public:
    explicit MySqlAccountRepository(std::shared_ptr<sql::Connection> conn)
       : m_conn(std::move(conn)) {}

    bool lockBalance(const UserId userId, const Decimal amount) override {
        std::lock_guard<std::mutex> lock(DatabaseManager::dbMutex());
    PrepStatementPtr pstmt(m_conn->prepareStatement(
        "UPDATE accounts SET balance_cash = balance_cash - ?, balance_reserved = balance_reserved + ? "
        "WHERE user_id = ? AND balance_cash >= ?"
    ));
    const std::string amtStr = amount.toString();
    pstmt->setString(1, amtStr);
    pstmt->setString(2, amtStr);
    pstmt->setUInt64(3, userId);
    pstmt->setString(4, amtStr);

    return pstmt->executeUpdate() > 0;
}

    void unlockBalance(const UserId userId, const Decimal amount) override {
        std::lock_guard<std::mutex> lock(DatabaseManager::dbMutex());
        PrepStatementPtr pstmt(m_conn->prepareStatement(
            "UPDATE accounts SET balance_cash = balance_cash + ?, balance_reserved = balance_reserved - ? "
            "WHERE user_id = ?"
        ));
        const std::string amtStr = amount.toString();
        pstmt->setString(1, amtStr);
        pstmt->setString(2, amtStr);
        pstmt->setUInt64(3, userId);
        pstmt->executeUpdate();
    }

    bool lockAsset(const UserId userId, const InstrumentId instId, const Decimal qty) override {
        std::lock_guard<std::mutex> lock(DatabaseManager::dbMutex());
        PrepStatementPtr pstmt(m_conn->prepareStatement(
            "UPDATE positions SET quantity = quantity - ?, quantity_reserved = quantity_reserved + ? "
            "WHERE instrument_id = ? AND account_id = (SELECT id FROM accounts WHERE user_id = ?) AND quantity >= ?"
        ));
        const std::string qtyStr = qty.toString();
        pstmt->setString(1, qtyStr);
        pstmt->setString(2, qtyStr);
        pstmt->setUInt64(3, instId);
        pstmt->setUInt64(4, userId);
        pstmt->setString(5, qtyStr);

        return pstmt->executeUpdate() > 0;
    }

    void unlockAsset(const UserId userId, const InstrumentId instId, const Decimal qty) override {
        std::lock_guard<std::mutex> lock(DatabaseManager::dbMutex());
        PrepStatementPtr pstmt(m_conn->prepareStatement(
            "UPDATE positions SET quantity = quantity + ?, quantity_reserved = quantity_reserved - ? "
            "WHERE instrument_id = ? AND account_id = (SELECT id FROM accounts WHERE user_id = ?)"
        ));

        const std::string qtyStr = qty.toString();
        pstmt->setString(1, qtyStr);
        pstmt->setString(2, qtyStr);
        pstmt->setUInt64(3, instId);
        pstmt->setUInt64(4, userId);

        pstmt->executeUpdate();
    }

    void settleTrade(const UserId buyerId, const UserId sellerId, const InstrumentId instId, const Decimal qty, const Decimal price, TradeId tradeId) override {
        std::lock_guard<std::mutex> lock(DatabaseManager::dbMutex());
        const Decimal totalCost = price * qty;

        updateBalanceReserved(buyerId, -totalCost);
        updatePosition(buyerId, instId, qty, price);

        const AccountId buyerAccId = getAccountIdByUserId(buyerId);
        addHistoryEntry(buyerAccId, -totalCost, "TRADE_BUY", tradeId);

        updateAssetReserved(sellerId, instId, -qty);
        changeBalance(sellerId, totalCost);

        const AccountId sellerAccId = getAccountIdByUserId(sellerId);
        addHistoryEntry(sellerAccId, totalCost, "TRADE_SELL", tradeId);
    }

    // BALANCE

    std::optional<Decimal> getBalance(const UserId userId) override {
        std::lock_guard<std::mutex> lock(DatabaseManager::dbMutex());
        PrepStatementPtr pstmt(m_conn->prepareStatement(
            "SELECT balance_cash FROM accounts WHERE user_id = ?"
        ));
        pstmt->setUInt64(1, userId);
        ResultSetPtr res(pstmt->executeQuery());

        if (res->next()) {
            return fromSql(res->getString(1));
        }
        return std::nullopt;
    }

    void updateBalance(const UserId userId, const Decimal newBalance) override {
        std::lock_guard<std::mutex> lock(DatabaseManager::dbMutex());
        PrepStatementPtr pstmt(m_conn->prepareStatement(
            "UPDATE accounts SET balance_cash = ? WHERE user_id = ?"
        ));
        pstmt->setString(1, toSql(newBalance));
        pstmt->setUInt64(2, userId);
        pstmt->executeUpdate();
    }

    void changeBalance(const UserId userId, const Decimal delta) override {
        std::lock_guard<std::mutex> lock(DatabaseManager::dbMutex());
        PrepStatementPtr pstmt(m_conn->prepareStatement(
            "UPDATE accounts SET balance_cash = balance_cash + ? WHERE user_id = ?"
        ));
        pstmt->setString(1, toSql(delta));
        pstmt->setUInt64(2, userId);
        pstmt->executeUpdate();
    }

    // POSITIONS

    void addPosition(const uint64_t userId, const uint64_t instId, const Decimal qty) override {
        auto account_id = getAccountIdByUserId(userId);

        PrepStatementPtr pstmt(m_conn->prepareStatement(
            "INSERT INTO positions (account_id, instrument_id, quantity) VALUES (?, ?, ?) "
            "ON DUPLICATE KEY UPDATE quantity = quantity + ?"
        ));
        pstmt->setUInt64(1, account_id);
        pstmt->setUInt64(2, instId);
        pstmt->setString(3, qty.toString());
        pstmt->setString(4, qty.toString());
        pstmt->executeUpdate();
    }

    std::optional<Position> getPosition(const UserId userId, const InstrumentId instrumentId) override {
        std::lock_guard<std::mutex> lock(DatabaseManager::dbMutex());
        PrepStatementPtr pstmt(m_conn->prepareStatement(
            "SELECT p.quantity, p.average_price "
            "FROM positions p "
            "JOIN accounts a ON p.account_id = a.id "
            "WHERE a.user_id = ? AND p.instrument_id = ?"
        ));
        pstmt->setUInt64(1, userId);
        pstmt->setUInt64(2, instrumentId);

        if (ResultSetPtr res(pstmt->executeQuery()); res->next()) {
            return Position{
                instrumentId,
                fromSql(res->getString("quantity")),
                fromSql(res->getString("average_price"))
            };
        }
        return std::nullopt;
    }

    void updatePosition(const UserId userId,
                    const InstrumentId instrumentId,
                    const Decimal quantityDelta,
                    const Decimal price) override
    {
        std::lock_guard<std::mutex> lock(DatabaseManager::dbMutex());
        PrepStatementPtr pstmt(m_conn->prepareStatement(
            "INSERT INTO positions (account_id, instrument_id, quantity, average_price) "
            "SELECT id, ?, ?, ? FROM accounts WHERE user_id = ? "
            "ON DUPLICATE KEY UPDATE "

            // обновляем количество
            "quantity = quantity + ?, "

            // обновляем среднюю цену
            "average_price = CASE "

            // покупка
            "WHEN ? > 0 THEN "
            "  (average_price * quantity + ? * ?) / (quantity + ?) "

            // полное закрытие
            "WHEN quantity + ? = 0 THEN 0 "

            // частичная продажа
            "ELSE average_price "

            "END"
        ));

        const std::string qStr = toSql(quantityDelta);
        const std::string pStr = toSql(price);

        // INSERT
        pstmt->setUInt64(1, instrumentId);
        pstmt->setString(2, qStr);
        pstmt->setString(3, pStr);
        pstmt->setUInt64(4, userId);

        // UPDATE

        // quantity
        pstmt->setString(5, qStr);

        // WHEN ? > 0
        pstmt->setString(6, qStr);

        // weighted avg
        pstmt->setString(7, qStr);
        pstmt->setString(8, pStr);
        pstmt->setString(9, qStr);

        // WHEN quantity + ? = 0
        pstmt->setString(10, qStr);

        pstmt->executeUpdate();
    }

    std::vector<Position> getPositions(const UserId userId) override {
        std::lock_guard<std::mutex> lock(DatabaseManager::dbMutex());
        PrepStatementPtr pstmt(m_conn->prepareStatement(
            "SELECT p.instrument_id, p.quantity, p.average_price "
            "FROM positions p "
            "JOIN accounts a ON p.account_id = a.id "
            "WHERE a.user_id = ?"
        ));
        pstmt->setUInt64(1, userId);
        ResultSetPtr res(pstmt->executeQuery());

        std::vector<Position> positions;
        positions.reserve(res->rowsCount());

        while (res->next()) {
            positions.push_back({
                res->getUInt64("instrument_id"),
                fromSql(res->getString("quantity")),
                fromSql(res->getString("average_price"))
            });
        }

        return positions;
    }

    // SYSTEM STATUS

    void setSystemStatus(const bool running) override {
        std::lock_guard<std::mutex> lock(DatabaseManager::dbMutex());
        PrepStatementPtr pstmt(m_conn->prepareStatement("UPDATE system_state SET trading_status = ?, id = 1"));
        pstmt->setString(1, running ? "RUNNING" : "STOPPED");
        pstmt->executeUpdate();
    }

    bool isSystemRunning() override {
        std::lock_guard<std::mutex> lock(DatabaseManager::dbMutex());
        if (ResultSetPtr res(m_conn->createStatement()->executeQuery("SELECT trading_status FROM system_state WHERE id = 1")); res->next()) return res->getString("trading_status") == "RUNNING";
        return false;
    }

    // BALANCE HISTORY

    std::vector<BalanceHistoryEntry> getHistory(const uint64_t accountId) override {
        std::lock_guard<std::mutex> lock(DatabaseManager::dbMutex());
        std::vector<BalanceHistoryEntry> history;

        PrepStatementPtr pstmt(m_conn->prepareStatement(
                "SELECT change_amount, reason, reference_id, created_at "
                "FROM balance_history "
                "WHERE account_id = ? "
                "ORDER BY created_at DESC "
                "LIMIT 100"));

        pstmt->setUInt64(1, accountId);

        ResultSetPtr res(pstmt->executeQuery());
        history.reserve(res->rowsCount());

        while (res->next()) {
            BalanceHistoryEntry entry;

            entry.change_amount = fromSql(res->getString("change_amount"));
            entry.reason = res->getString("reason");
            if (!res->isNull("reference_id")) {
                entry.reference_id = res->getUInt64("reference_id");
            } else {
                entry.reference_id = std::nullopt;
            }
            entry.timestamp = timeFromSqlString(res->getString("created_at"));

            history.push_back(std::move(entry));
        }

        return history;
    }

    void addHistoryEntry(const uint64_t accountId, const Decimal amount, const std::string& reason, const std::optional<uint64_t> referenceId) override {
        std::lock_guard<std::mutex> lock(DatabaseManager::dbMutex());
        PrepStatementPtr pstmt(m_conn->prepareStatement(
                "INSERT INTO balance_history (account_id, change_amount, reason, reference_id) "
                "VALUES (?, ?, ?, ?)"));

        pstmt->setUInt64(1, accountId);
        pstmt->setString(2, amount.toString());
        pstmt->setString(3, reason);
        if (referenceId.has_value()) {
            pstmt->setUInt64(4, referenceId.value());
        } else {
            pstmt->setNull(4, sql::DataType::BIGINT);
        }

        pstmt->executeUpdate();
    }

    // HELPERS

    AccountId getAccountIdByUserId(const uint64_t userId) override {
        std::lock_guard<std::mutex> lock(DatabaseManager::dbMutex());
        PrepStatementPtr pstmt(m_conn->prepareStatement(
                "SELECT id FROM accounts WHERE user_id = ?"));
        pstmt->setUInt64(1, userId);

        if (ResultSetPtr res(pstmt->executeQuery()); res->next()) {
            return res->getUInt64("id");
        }

        throw std::runtime_error("Account not found for user_id: " + std::to_string(userId)); // Account must exist cause we have trigger
    }
};