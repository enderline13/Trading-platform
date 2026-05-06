#include "core/MarketDataManager.h"

void MarketDataManager::onTrade(InstrumentId instrument_id, const common::Trade& trade) {
    getInstrument(instrument_id)->addTrade(trade);
    spdlog::debug("MarketData: Trade processed for instrument {}", instrument_id);
}

void MarketDataManager::onBookUpdate(const InstrumentId instrument_id, const market::OrderBook& book) {
    getInstrument(instrument_id)->updateBook(book);
}

std::shared_ptr<InstrumentData> MarketDataManager::getInstrument(InstrumentId id) {
    std::lock_guard lock(m_mutex);
    auto it = m_instruments.find(id);
    if (it == m_instruments.end()) {
        it = m_instruments.emplace(id, std::make_shared<InstrumentData>()).first;
    }
    return it->second;
}