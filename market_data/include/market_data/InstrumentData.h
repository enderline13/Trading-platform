#pragma once

#include <deque>
#include <mutex>

#include "CandleAggregator.h"
#include "market.pb.h"
#include "common/ProtoMapper.h"

template<typename T>
struct Subscription {
    uint64_t id;
    std::function<void(const T&)> callback;
};

class InstrumentData {
public:
    void updateBook(const market::OrderBook& book) {
        {
            std::lock_guard lock(m_book_mutex);
            m_current_book = book;
        }
        std::lock_guard lock(m_sub_mutex);
        for (auto& [id, sub] : m_book_subs) {
            sub(book);
        }
    }

    void addTrade(const common::Trade& trade) {
        {
            std::lock_guard lock(m_trades_mutex);
            m_recent_trades.push_front(trade);
            if (m_recent_trades.size() > 50) m_recent_trades.pop_back();
        }

        auto tp = std::chrono::system_clock::from_time_t(trade.executed_at().seconds());
        auto time_t = std::chrono::system_clock::to_time_t(tp);
        auto minute_t = (time_t / 60) * 60;
        auto minute_tp = std::chrono::system_clock::from_time_t(minute_t);
        // Проверяем, изменилась ли минута
        bool newMinute = false;
        {
            // CandleAggregator должен предоставлять доступ к последней свече
            // Предположим, у него есть метод getLastCandleTimestamp()
            auto lastTs = m_candles.getLastCandleTimestamp(); // надо добавить метод
            newMinute = (lastTs.time_since_epoch().count() == 0) || (lastTs != minute_tp);
        }

        m_candles.addTrade(mapper::fromProto(trade.price()), mapper::fromProto(trade.quantity()), tp);

        if (newMinute) {
            auto history = m_candles.getHistory();
            if (history.size() >= 2) { // есть как минимум предыдущая свеча
                const auto& finishedCandle = history[history.size() - 2]; // предпоследняя
                common::Candle protoCandle = mapper::toProto(finishedCandle);
                std::lock_guard lock(m_sub_mutex);
                for (auto& [id, sub] : m_candle_subs) {
                    sub(protoCandle);
                }
            }
        }
        auto history = m_candles.getHistory();
        if (!history.empty()) {
            const auto& lastCandle = history.back();

            common::Candle protoCandle;
            *protoCandle.mutable_time()   = mapper::toProtoTimestamp(lastCandle.timestamp);
            *protoCandle.mutable_open()   = mapper::toProto(lastCandle.open);
            *protoCandle.mutable_high()   = mapper::toProto(lastCandle.high);
            *protoCandle.mutable_low()    = mapper::toProto(lastCandle.low);
            *protoCandle.mutable_close()  = mapper::toProto(lastCandle.close);
            *protoCandle.mutable_volume() = mapper::toProto(lastCandle.volume);

            std::lock_guard lock(m_sub_mutex);
            for (auto& [id, sub] : m_candle_subs) {
                sub(protoCandle);
            }
        }

        std::lock_guard lock(m_sub_mutex);
        for (auto& [id, sub] : m_trade_subs) {
            sub(trade);
        }
    }

    uint64_t subscribeTrades(std::function<void(const common::Trade&)> cb) {
        std::lock_guard lock(m_sub_mutex);
        uint64_t id = m_next_sub_id++;
        m_trade_subs[id] = std::move(cb);
        return id;
    }

    void unsubscribeTrades(uint64_t id) {
        std::lock_guard lock(m_sub_mutex);
        m_trade_subs.erase(id);
    }

    uint64_t subscribeBook(std::function<void(const market::OrderBook&)> cb) {
        std::lock_guard lock(m_sub_mutex);
        uint64_t id = m_next_sub_id++;
        m_book_subs[id] = std::move(cb);
        return id;
    }

    void unsubscribeBook(uint64_t id) {
        std::lock_guard lock(m_sub_mutex);
        m_book_subs.erase(id);
    }

    market::OrderBook getBookSnapshot() const {
        std::lock_guard lock(m_book_mutex);
        return m_current_book;
    }

    std::vector<common::Trade> getTradesSnapshot() const {
        std::lock_guard lock(m_trades_mutex);
        return {m_recent_trades.begin(), m_recent_trades.end()};
    }

    std::vector<Candle> getCandles() const {
        return m_candles.getHistory();
    }

    uint64_t subscribeCandles(std::function<void(const common::Candle&)> cb) {
        std::lock_guard lock(m_sub_mutex);
        uint64_t id = m_next_sub_id++;
        m_candle_subs[id] = std::move(cb);
        return id;
    }

    void unsubscribeCandles(uint64_t id) {
        std::lock_guard lock(m_sub_mutex);
        m_candle_subs.erase(id);
    }

private:
    mutable std::mutex m_book_mutex;
    market::OrderBook m_current_book;

    mutable std::mutex m_trades_mutex;
    std::deque<common::Trade> m_recent_trades;

    CandleAggregator m_candles;

    std::mutex m_sub_mutex;
    uint64_t m_next_sub_id = 1;
    std::map<uint64_t, std::function<void(const common::Trade&)>> m_trade_subs;
    std::map<uint64_t, std::function<void(const market::OrderBook&)>> m_book_subs;
    std::map<uint64_t, std::function<void(const common::Candle&)>> m_candle_subs;
};