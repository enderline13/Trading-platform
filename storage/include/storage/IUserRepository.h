#pragma once

#pragma once

#include <optional>
#include <mutex>
#include <unordered_map>

#include "mysql/jdbc.h"

#include "common/Types.h"
#include "common/User.h"

class IUserRepository {
public:
    virtual std::optional<User> getById(UserId) = 0;
    virtual std::optional<User> getByEmail(const std::string&) = 0;
    virtual std::optional<User> getByUsername(const std::string&) = 0; // Добавлено
    virtual UserId create(const User&) = 0;
};

class InMemoryUserRepository : public IUserRepository {
public:
    UserId create(const User& user) override {
        std::lock_guard lock(m_mutex);

        User copy = user;
        copy.id = m_nextId++;

        m_users[copy.id] = copy;
        m_emailIndex[copy.email] = copy.id;
        m_usernameIndex[copy.username] = copy.id; // Новый индекс

        return copy.id;
    }

    std::optional<User> getById(UserId id) override {
        std::lock_guard lock(m_mutex);
        auto it = m_users.find(id);
        if (it == m_users.end()) return std::nullopt;
        return it->second;
    }

    std::optional<User> getByEmail(const std::string& email) override {
        std::lock_guard lock(m_mutex);
        auto it = m_emailIndex.find(email);
        if (it == m_emailIndex.end()) return std::nullopt;
        return m_users[it->second];
    }

    // Реализация нового метода интерфейса
    std::optional<User> getByUsername(const std::string& username) override {
        std::lock_guard lock(m_mutex);
        auto it = m_usernameIndex.find(username);
        if (it == m_usernameIndex.end()) return std::nullopt;
        return m_users[it->second];
    }

private:
    std::unordered_map<UserId, User> m_users;
    std::unordered_map<std::string, UserId> m_emailIndex;
    std::unordered_map<std::string, UserId> m_usernameIndex; // Для быстрого логина

    UserId m_nextId{1};
    mutable std::mutex m_mutex;
};

class MySqlUserRepository : public IUserRepository {
    std::shared_ptr<sql::Connection> m_conn;
public:
    MySqlUserRepository(std::shared_ptr<sql::Connection> conn) : m_conn(conn) {}

    std::optional<User> getByField(const std::string& field, const std::string& value) {
        auto pstmt = m_conn->prepareStatement("SELECT id, email, username, password_hash FROM users WHERE " + field + " = ?");
        pstmt->setString(1, value);
        auto res = pstmt->executeQuery();

        if (res->next()) {
            return User{res->getUInt64("id"), res->getString("username"), res->getString("email"), res->getString("password_hash")};
        }
        return std::nullopt;
    }

    std::optional<User> getById(UserId id) override { return getByField("id", std::to_string(id)); }
    std::optional<User> getByEmail(const std::string& email) override { return getByField("email", email); }
    std::optional<User> getByUsername(const std::string& username) override { return getByField("username", username); }

    UserId create(const User& user) override {
        auto pstmt = m_conn->prepareStatement("INSERT INTO users (email, password_hash, username) VALUES (?, ?, ?)");
        pstmt->setString(1, user.email);
        pstmt->setString(2, user.password_hash);
        pstmt->setString(3, user.username);
        pstmt->executeUpdate();

        auto res = m_conn->createStatement()->executeQuery("SELECT LAST_INSERT_ID()");
        res->next();
        return res->getInt64(1);
    }
};