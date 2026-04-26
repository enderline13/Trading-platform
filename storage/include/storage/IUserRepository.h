#pragma once

#pragma once

#include <optional>

#include "mysql/jdbc.h"

#include "common/Types.h"
#include "common/User.h"
#include "utils.h"

class IUserRepository {
public:
    virtual ~IUserRepository() = default;

    virtual std::optional<User> getById(UserId) = 0;
    virtual std::optional<User> getByEmail(const std::string&) = 0;
    virtual std::optional<User> getByUsername(const std::string&) = 0;
    virtual UserId create(const User&) = 0;
};

class MySqlUserRepository final : public IUserRepository {
private:
    std::shared_ptr<sql::Connection> m_conn;

    std::optional<User> getByField(const std::string& field, const std::string& value) const {
        PrepStatementPtr pstmt(m_conn->prepareStatement("SELECT id, email, username, password_hash, role FROM users WHERE " + field + " = ?"));
        pstmt->setString(1, value);

        if (ResultSetPtr res(pstmt->executeQuery()); res->next()) {
            auto temp_role = res->getString("role") == "ADMIN" ? User::Role::ADMIN : User::Role::USER;
            return User{res->getUInt64("id"),
                res->getString("username"),
                res->getString("email"),
                res->getString("password_hash"),
                temp_role};
        }
        return std::nullopt;
    }

public:
    explicit MySqlUserRepository(std::shared_ptr<sql::Connection> conn) : m_conn(std::move(conn)) {}

    std::optional<User> getById(const UserId id) override { return getByField("id", std::to_string(id)); }
    std::optional<User> getByEmail(const std::string& email) override { return getByField("email", email); }
    std::optional<User> getByUsername(const std::string& username) override { return getByField("username", username); }

    UserId create(const User& user) override {
        PrepStatementPtr pstmt(m_conn->prepareStatement("INSERT INTO users (email, password_hash, username) VALUES (?, ?, ?)"));
        pstmt->setString(1, user.email);
        pstmt->setString(2, user.password_hash);
        pstmt->setString(3, user.username);
        pstmt->executeUpdate();

        if (ResultSetPtr res(m_conn->createStatement()->executeQuery("SELECT LAST_INSERT_ID()")); res->next()) {
            return res->getUInt64(1);
        }

        throw std::runtime_error("Failed to retrieve last insert ID for user");
    }
};