#pragma once

#include "Order.h"
#include "Trade.h"
#include "Decimal.h"
#include "Types.h"

struct PartialFill {
    OrderId order_id;
    Decimal remaining_qty;
};

struct MatchResult {
    Order::Status status;
    std::vector<Trade> trades;
    std::vector<OrderId> filled_order_ids;
    std::vector<PartialFill> partial_fills;
};