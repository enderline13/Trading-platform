#pragma once

#include "common/Order.h"
#include "common/Trade.h"
#include "common/Types.h"

class Storage {
public:
    virtual void saveOrder(const Order&) = 0;
    virtual void saveTrade(const Trade&) = 0;
    virtual void updateBalance(...) = 0;

    virtual std::vector<Order> getUserOrders(UserId) = 0;
};

class MySQLStorage : public Storage {

};