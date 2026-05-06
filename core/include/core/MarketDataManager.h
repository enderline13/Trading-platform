#pragma once

#include <unordered_map>

#include <spdlog/spdlog.h>

#include "market_data/InstrumentData.h"
#include "common/Types.h"

class MarketDataManager {
public:
    void onTrade(InstrumentId instrument_id, const common::Trade& trade);

    void onBookUpdate(InstrumentId instrument_id, const market::OrderBook& book);

    // Методы для получения данных
    std::shared_ptr<InstrumentData> getInstrument(InstrumentId id);

private:
    std::mutex m_mutex;
    std::unordered_map<InstrumentId, std::shared_ptr<InstrumentData>> m_instruments;
};