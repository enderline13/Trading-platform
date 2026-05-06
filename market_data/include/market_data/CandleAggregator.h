#pragma once

#include <vector>
#include <mutex>

#include "common/Decimal.h"
#include "common/Candle.h"

class CandleAggregator {
public:
    void addTrade(const Decimal price, const Decimal quantity, const std::chrono::system_clock::time_point tp) {
        std::lock_guard lock(m_mutex);

        const auto time_t = std::chrono::system_clock::to_time_t(tp);
        const auto minute_t = (time_t / 60) * 60;
        const auto minute_tp = std::chrono::system_clock::from_time_t(minute_t);

        if (m_candles.empty() || m_candles.back().timestamp != minute_tp) {
            m_candles.push_back({minute_tp, price, price, price, price, quantity});
        } else {
            auto& c = m_candles.back();
            if (price > c.high) c.high = price;
            if (price < c.low) c.low = price;
            c.close = price;
            c.volume = c.volume + quantity;
        }
    }

    std::vector<Candle> getHistory() const {
        std::lock_guard lock(m_mutex);
        return m_candles;
    }

    std::chrono::system_clock::time_point getLastCandleTimestamp() const {
        std::lock_guard lock(m_mutex);
        if (m_candles.empty()) return {};
        return m_candles.back().timestamp;
    }

private:
    mutable std::mutex m_mutex;
    std::vector<Candle> m_candles;
};