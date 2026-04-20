#pragma once

#include <memory>

namespace sql
{
    class Connection;
}

class TransactionGuard {
    std::shared_ptr<sql::Connection> m_conn;
    bool m_committed = false;
public:
    TransactionGuard(std::shared_ptr<sql::Connection> conn) : m_conn(conn) {
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
