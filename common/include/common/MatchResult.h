#pragma once

#include "common/Order.h"
#include "common/Trade.h"

struct MatchResult {
    Order::Status status;
    std::vector<Trade> trades;
    std::vector<OrderId> filled_order_ids;
};