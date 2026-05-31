#pragma once

#include <memory>

namespace sql
{
    class Connection;
}

class TransactionGuard {
    std::shared_ptr<sql::Connection> m_conn;
    bool m_committed = false;
    std::unique_lock<std::recursive_mutex> m_lock;
public:
    explicit TransactionGuard(std::shared_ptr<sql::Connection> conn) : m_conn(std::move(conn)), m_lock(DatabaseManager::dbMutex()) {
        m_conn->setAutoCommit(false);
    }
    ~TransactionGuard() {
        if (!m_committed) m_conn->rollback();
        m_conn->setAutoCommit(true);
    }
    void commit() {
        m_conn->commit();
        m_committed = true;
    }
};
