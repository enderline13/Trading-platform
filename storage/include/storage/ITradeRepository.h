#pragma once

#include <mutex>
#include <unordered_map>

#include "common/Types.h"
#include "common/Trade.h"

class ITradeRepository {
public:
    virtual void save(const Trade& trade, UserId buyer, UserId seller) = 0;
    virtual std::vector<Trade> getByUser(UserId userId, std::optional<InstrumentId> instId = std::nullopt) = 0;
};

class InMemoryTradeRepository : public ITradeRepository {
public:
    void save(const Trade& trade, UserId buyer, UserId seller) override {
        std::lock_guard lock(m_mutex);

        m_allTrades.push_back(trade);

        // Индексируем по переданным ID
        m_userTrades[buyer].push_back(trade);
        m_userTrades[seller].push_back(trade);
    }

    std::vector<Trade> getByUser(UserId userId, std::optional<InstrumentId> instId) override {
        std::lock_guard lock(m_mutex);

        // Если у пользователя вообще нет сделок
        if (!m_userTrades.contains(userId)) {
            return {};
        }

        const auto& userHistory = m_userTrades.at(userId);

        // Если фильтр по инструменту не задан — отдаем всю историю пользователя
        if (!instId.has_value()) {
            return userHistory;
        }

        // Если фильтр есть — фильтруем только сделки ЭТОГО пользователя (уже быстрее)
        std::vector<Trade> result;
        for (const auto& trade : userHistory) { // Исправленный цикл без лишних bindings
            if (trade.instrument_id == *instId) {
                result.push_back(trade);
            }
        }

        return result;
    }

private:
    std::vector<Trade> m_allTrades;

    // Индекс: UserId -> Список его сделок
    std::unordered_map<UserId, std::vector<Trade>> m_userTrades;

    mutable std::mutex m_mutex;
};