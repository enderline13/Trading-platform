#pragma once

#pragma once

#include <optional>
#include <mutex>
#include <unordered_map>

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