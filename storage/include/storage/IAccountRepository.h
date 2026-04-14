#pragma once

#include <mutex>
#include <unordered_map>

#include "common/Types.h"
#include "common/Decimal.h"
#include "common/Position.h"

class IAccountRepository {
public:
    virtual ~IAccountRepository() = default;

    virtual Decimal getBalance(UserId userId) = 0;
    virtual void updateBalance(UserId userId, Decimal newBalance) = 0;
    virtual void changeBalance(UserId userId, Decimal delta) = 0;

    virtual std::optional<Position> getPosition(UserId userId, InstrumentId instrumentId) = 0;
    virtual void updatePosition(UserId userId, InstrumentId instrumentId, Decimal quantityDelta, Decimal price) = 0;
    virtual std::vector<Position> getPositions(UserId userId) = 0;
};

class InMemoryAccountRepository : public IAccountRepository {
public:
    // --- Деньги ---
    Decimal getBalance(UserId userId) override {
        std::lock_guard lock(m_mutex);
        return m_balances[userId];
    }

    void updateBalance(UserId userId, Decimal newBalance) override {
        std::lock_guard lock(m_mutex);
        m_balances[userId] = newBalance;
    }

    // --- Позиции ---
    std::optional<Position> getPosition(UserId userId, InstrumentId instrumentId) override {
        std::lock_guard lock(m_mutex);
        auto it = m_positions[userId].find(instrumentId);
        if (it == m_positions[userId].end()) return std::nullopt;
        return it->second;
    }

    void updatePosition(UserId userId, InstrumentId instrumentId, Decimal quantityDelta, Decimal price) override {
        std::lock_guard lock(m_mutex);
        auto& pos = m_positions[userId][instrumentId];

        pos.instrument_id = instrumentId;

        // Логика обновления средней цены при покупке
        if (quantityDelta > Decimal{0,0}) {
            Decimal totalCost = (pos.average_price * pos.quantity) + (price * quantityDelta);
            pos.quantity += quantityDelta;
            pos.average_price = totalCost / pos.quantity; // Убедись, что оператор / перегружен
        } else {
            // При продаже средняя цена обычно не меняется, просто уменьшается количество
            pos.quantity += quantityDelta;
        }

        // Если количество стало 0, можно либо оставить запись, либо удалить
        if (pos.quantity == Decimal{0,0}) {
            m_positions[userId].erase(instrumentId);
        }
    }

    std::vector<Position> getPositions(UserId userId) override {
        std::lock_guard lock(m_mutex);
        std::vector<Position> result;
        for (const auto& [id, pos] : m_positions[userId]) {
            result.push_back(pos);
        }
        return result;
    }

    void changeBalance(UserId userId, Decimal delta) {
        std::lock_guard lock(m_mutex);
        m_balances[userId] += delta;
    }

private:
    mutable std::mutex m_mutex;
    std::unordered_map<UserId, Decimal> m_balances;
    // Карта: UserId -> (InstrumentId -> Position)
    std::unordered_map<UserId, std::unordered_map<InstrumentId, Position>> m_positions;
};