#pragma once

#include "Decimal.h"

struct Position {
    uint64_t instrument_id = 0;
    Decimal quantity;
    Decimal average_price;
};