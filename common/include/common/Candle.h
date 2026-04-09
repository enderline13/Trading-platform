#pragma once

#include "Decimal.h"
#include "Types.h"

struct Candle {
    Timestamp time;
    Decimal open;
    Decimal high;
    Decimal low;
    Decimal close;
    Decimal volume;
};