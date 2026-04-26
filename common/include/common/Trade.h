#pragma once

#include "Decimal.h"
#include "Types.h"

using TradeId = uint64_t;

struct Trade {
    uint64_t id = 0;
    uint64_t instrument_id = 0;
    uint64_t buy_order_id = 0;
    uint64_t sell_order_id = 0;

    Decimal price;
    Decimal quantity;

    Timestamp executed_at;
};