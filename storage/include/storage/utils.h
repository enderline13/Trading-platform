#pragma once

#include <memory>

#include "mysql/jdbc.h"

using StatementPtr = const std::unique_ptr<sql::Statement>;
using PrepStatementPtr = const std::unique_ptr<sql::PreparedStatement>;
using ResultSetPtr = const std::unique_ptr<sql::ResultSet>;