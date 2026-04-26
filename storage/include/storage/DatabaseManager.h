#pragma once

#include <memory>
#include <string>

#include "jdbc/mysql_driver.h"
#include "jdbc/cppconn/connection.h"
#include "jdbc/cppconn/driver.h"

class DatabaseManager final {
public:
    static DatabaseManager& instance() {
        static DatabaseManager inst;
        return inst;
    }

    void init(const std::string& host, const std::string& user, const std::string& pass, const std::string& db) {
        m_driver = sql::mysql::get_mysql_driver_instance();
        m_connection.reset(m_driver->connect(host, user, pass));
        m_connection->setSchema(db);
    }

    std::shared_ptr<sql::Connection> getConnection() {
        return m_connection;
    }

private:
    DatabaseManager() = default;
    sql::Driver* m_driver = nullptr;
    std::shared_ptr<sql::Connection> m_connection;
};
