#pragma once

#include "common/Trade.h"
#include "common/Types.h"

class MarketDataService {
public:
    void onTrade(const Trade&);
    void onOrderBookChange(InstrumentId);
    //OrderBook getSnapshot(InstrumentId);
};
