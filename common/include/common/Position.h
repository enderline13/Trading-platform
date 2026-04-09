#pragma once

#include "Decimal.h"

struct Position {
    int64_t instrument_id = 0;
    Decimal quantity;
    Decimal average_price;
};