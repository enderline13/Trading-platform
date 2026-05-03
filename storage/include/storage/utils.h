#pragma once

#include <memory>

#include "mysql/jdbc.h"

using StatementPtr = const std::unique_ptr<sql::Statement>;
using PrepStatementPtr = const std::unique_ptr<sql::PreparedStatement>;
using ResultSetPtr = const std::unique_ptr<sql::ResultSet>;

inline std::chrono::system_clock::time_point timeFromSqlString(const std::string& sqlTime) {
    if (sqlTime.empty()) return {};

    std::tm tm = {};
    std::istringstream ss(sqlTime);

    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

    if (ss.fail()) {
        return std::chrono::system_clock::now();
    }

    time_t tt = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(tt);
}