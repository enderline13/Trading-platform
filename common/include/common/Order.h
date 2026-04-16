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

inline std::string orderTypeToString(Order::Type type) {
    switch (type) {
        case Order::Type::LIMIT:
            return "LIMIT";
        case Order::Type::MARKET:
            return "MARKET";
        case Order::Type::STOP:
            return "STOP";
        default:
            return "";
    }
}

inline std::string orderSideToString(Order::Side side) {
    switch (side) {
        case Order::Side::BUY:
            return "BUY";
        case Order::Side::SELL:
            return "SELL";
        default:
            return "";
    }
}

inline std::string orderStatusToString(Order::Status status) {
    switch (status) {
        case Order::Status::NEW:
            return "NEW";
        case Order::Status::PARTIALLY_FILLED:
            return "PARTIALLY_FILLED";
        case Order::Status::FILLED:
            return "FILLED";
        case Order::Status::CANCELED:
            return "CANCELED";
        case Order::Status::REJECTED:
            return "REJECTED";
        default:
            return "";
    }
}

using OrderSide = Order::Side;

inline OrderSide stringToOrderSide(const std::string& str) {
    if (str == "BUY")  return OrderSide::BUY;
    if (str == "SELL") return OrderSide::SELL;
    throw std::runtime_error("Unknown OrderSide string: " + str);
}

using OrderType = Order::Type;
inline OrderType stringToOrderType(const std::string& str) {
    if (str == "LIMIT")  return OrderType::LIMIT;
    if (str == "MARKET") return OrderType::MARKET;
    if (str == "STOP") return OrderType::STOP;
    throw std::runtime_error("Unknown OrderType string: " + str);
}

using OrderStatus = Order::Status;
inline OrderStatus stringToOrderStatus(const std::string& str) {
    if (str == "NEW")              return OrderStatus::NEW;
    if (str == "PARTIALLY_FILLED") return OrderStatus::PARTIALLY_FILLED;
    if (str == "FILLED")           return OrderStatus::FILLED;
    if (str == "CANCELED")         return OrderStatus::CANCELED;
    if (str == "REJECTED")         return OrderStatus::REJECTED;
    throw std::runtime_error("Unknown OrderStatus string: " + str);
}