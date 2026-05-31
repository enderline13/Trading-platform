#pragma once

#pragma once

#include <optional>

#include "mysql/jdbc.h"

#include "common/Types.h"
#include "common/User.h"
#include "utils.h"
#include "DatabaseManager.h"

class IUserRepository {
public:
    virtual ~IUserRepository() = default;

    virtual std::optional<User> getById(UserId) = 0;
    virtual std::optional<User> getByEmail(const std::string&) = 0;
    virtual std::optional<User> getByUsername(const std::string&) = 0;
    virtual UserId create(const User&) = 0;
    virtual std::vector<User> getAllUsers() = 0;
    virtual void setUserRole(UserId userId, User::Role role) = 0;
    virtual void setUserActive(UserId userId, bool active) = 0;
};

class MySqlUserRepository final : public IUserRepository {
private:
    std::shared_ptr<sql::Connection> m_conn;

    std::optional<User> getByField(const std::string& field, const std::string& value) const {
        std::lock_guard<std::recursive_mutex> lock(DatabaseManager::dbMutex());
        PrepStatementPtr pstmt(m_conn->prepareStatement("SELECT id, email, username, password_hash, role, is_active FROM users WHERE " + field + " = ?"));
        pstmt->setString(1, value);

        if (ResultSetPtr res(pstmt->executeQuery()); res->next()) {
            User user;
            user.id = res->getUInt64("id");
            user.username = res->getString("username");
            user.email = res->getString("email");
            user.password_hash = res->getString("password_hash");
            user.role = (res->getString("role") == "ADMIN") ? User::Role::ADMIN : User::Role::USER;
            user.is_active = res->getBoolean("is_active");
            return user;
        }
        return std::nullopt;
    }

public:
    explicit MySqlUserRepository(std::shared_ptr<sql::Connection> conn) : m_conn(std::move(conn)) {}

    std::optional<User> getById(const UserId id) override { return getByField("id", std::to_string(id)); }
    std::optional<User> getByEmail(const std::string& email) override { return getByField("email", email); }
    std::optional<User> getByUsername(const std::string& username) override { return getByField("username", username); }

    UserId create(const User& user) override {
        std::lock_guard<std::recursive_mutex> lock(DatabaseManager::dbMutex());
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

    std::vector<User> getAllUsers() override {
        std::lock_guard<std::recursive_mutex> lock(DatabaseManager::dbMutex());
        StatementPtr stmt(m_conn->createStatement());
        ResultSetPtr res(stmt->executeQuery(
            "SELECT id, email, username, password_hash, role, is_active FROM users"
        ));
        std::vector<User> users;
        while (res->next()) {
            User u;
            u.id = res->getUInt64("id");
            u.username = res->getString("username");
            u.email = res->getString("email");
            u.password_hash = res->getString("password_hash");
            u.role = (res->getString("role") == "ADMIN") ? User::Role::ADMIN : User::Role::USER;
            u.is_active = res->getBoolean("is_active");
            users.push_back(u);
        }
        return users;
    }

    // setUserRole
    void setUserRole(const UserId userId, const User::Role role) override {
        std::lock_guard<std::recursive_mutex> lock(DatabaseManager::dbMutex());
        PrepStatementPtr pstmt(m_conn->prepareStatement(
            "UPDATE users SET role = ? WHERE id = ?"
        ));
        pstmt->setString(1, (role == User::Role::ADMIN) ? "ADMIN" : "USER");
        pstmt->setUInt64(2, userId);
        pstmt->executeUpdate();
    }

    // setUserActive
    void setUserActive(const UserId userId, const bool active) override {
        std::lock_guard<std::recursive_mutex> lock(DatabaseManager::dbMutex());
        PrepStatementPtr pstmt(m_conn->prepareStatement(
            "UPDATE users SET is_active = ? WHERE id = ?"
        ));
        pstmt->setBoolean(1, active);
        pstmt->setUInt64(2, userId);
        pstmt->executeUpdate();
    }
};