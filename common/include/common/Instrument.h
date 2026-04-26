#pragma once

#include "Decimal.h"
#include <string>

struct Instrument {
    uint64_t id = 0;
    std::string symbol;
    std::string name;
    Decimal tick_size;
    Decimal lot_size;
    bool is_active = false;
};