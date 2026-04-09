#pragma once

#include <vector>

#include "common/Types.h"
#include "common/Trade.h"
#include "common/Order.h"

class TradingCore {
public:
    OrderId placeOrder(const PlaceOrderRequest&, UserId);
    void cancelOrder(OrderId, UserId);

    std::vector<Order> getUserOrders(UserId);
    std::vector<Trade> getTradeHistory(UserId);
};