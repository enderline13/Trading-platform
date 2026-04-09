#pragma once

#include "common/Trade.h"
#include "common/Types.h"

class MarketDataService {
public:
    void onTrade(const Trade&);
    OrderBook getSnapshot(InstrumentId);
};
