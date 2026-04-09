#pragma once

#include "Decimal.h"
#include "Types.h"

struct Order {
    enum class Type {
        LIMIT = 0,
        MARKET = 1,
        STOP = 2
    };

    enum class Side {
        BUY = 0,
        SELL = 1
    };

    enum class Status {
        NEW = 0,
        PARTIALLY_FILLED = 1,
        FILLED = 2,
        CANCELED = 3,
        REJECTED = 4
    };

    uint64_t id = 0;
    uint64_t user_id = 0;
    uint64_t instrument_id = 0;

    Type type = Type::LIMIT;
    Side side = Side::BUY;

    Decimal price;
    Decimal quantity;
    Decimal remaining_quantity;

    Status status = Status::NEW;
    Timestamp created_at;
};