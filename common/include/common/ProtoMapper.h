#pragma once

#include "common.grpc.pb.h"
#include "admin.pb.h"
#include "trading.pb.h"
#include "common.pb.h"
#include "auth.pb.h"
#include <google/protobuf/util/time_util.h>

#include "common/Instrument.h"
#include "common/Order.h"
#include "common/Position.h"
#include "common/User.h"
#include "common/Trade.h"
#include "common/Candle.h"

struct AddPositionRequest {
    uint64_t user_id = 0;
    uint64_t instrument_id = 0;

    Decimal quantity;
};

namespace mapper {

    inline common::Decimal toProto(const Decimal& d) {
        common::Decimal pd;
        pd.set_units(d.units);
        pd.set_nanos(d.nanos);
        return pd;
    }
    inline Decimal fromProto(const common::Decimal& pd) {
        Decimal d;
        d.units = pd.units();
        d.nanos = pd.nanos();
        d.normalize();
        return d;
    }
    inline Instrument fromProto(const common::Instrument& pi) {
        Instrument i;
        i.id = pi.id();
        i.symbol = pi.symbol();
        i.name = pi.name();
        i.tick_size = fromProto(pi.tick_size());
        i.lot_size = fromProto(pi.lot_size());
        i.is_active = pi.is_active();
        return i;
    }

    inline common::Instrument toProto(const Instrument& i) {
        common::Instrument pi;
        pi.set_id(i.id);
        pi.set_symbol(i.symbol);
        pi.set_name(i.name);
        *pi.mutable_tick_size() = toProto(i.tick_size);
        *pi.mutable_lot_size() = toProto(i.lot_size);
        pi.set_is_active(i.is_active);
        return pi;
    }

    inline common::Position toProto(const Position& cp) {
        common::Position p;

        p.set_instrument_id(cp.instrument_id);

        auto* qty = p.mutable_quantity();
        qty->set_units(cp.quantity.units);
        qty->set_nanos(cp.quantity.nanos);

        auto* avg = p.mutable_average_price();
        avg->set_units(cp.average_price.units);
        avg->set_nanos(cp.average_price.nanos);

        return p;
    }

    inline auth::User::Role toProtoRole(const User::Role role) {
        switch (role) {
            case User::Role::USER:  return auth::User::USER;
            case User::Role::ADMIN: return auth::User::ADMIN;
        }

        return auth::User::USER;
    }

    inline auth::User toProto(const User& u) {
        auth::User pu;
        pu.set_id(u.id);
        pu.set_username(u.username);
        pu.set_email(u.email);
        pu.set_role(toProtoRole(u.role));
        return pu;
    }

    inline common::Order::OrderStatus toProto(const Order::Status status) {
        switch (status) {
            case Order::Status::NEW:  return common::Order::NEW;
            case Order::Status::PARTIALLY_FILLED:  return common::Order::PARTIALLY_FILLED;
            case Order::Status::FILLED:  return common::Order::FILLED;
            case Order::Status::CANCELED:  return common::Order::CANCELED;
            case Order::Status::REJECTED:  return common::Order::REJECTED;
        }

        return common::Order::NEW;
    }

    inline Order::Status fromProto(const common::Order::OrderStatus status ) {
        switch (status) {
            case common::Order::OrderStatus::Order_OrderStatus_NEW:  return Order::Status::NEW;
            case common::Order::OrderStatus::Order_OrderStatus_CANCELED:  return Order::Status::CANCELED;
            case common::Order::OrderStatus::Order_OrderStatus_REJECTED:  return Order::Status::REJECTED;
            case common::Order::OrderStatus::Order_OrderStatus_FILLED:  return Order::Status::FILLED;
            case common::Order::OrderStatus::Order_OrderStatus_PARTIALLY_FILLED:  return Order::Status::PARTIALLY_FILLED;
            default: return Order::Status::NEW;
        }

        return Order::Status::NEW;
    }

    using google::protobuf::util::TimeUtil;

    // --- Enums ---
    inline common::Order_OrderSide toProto(Order::Side side) {
        return side == Order::Side::BUY ? common::Order_OrderSide_BUY : common::Order_OrderSide_SELL;
    }

    inline Order::Side fromProto(common::Order_OrderSide side) {
        return side == common::Order_OrderSide_BUY ? Order::Side::BUY : Order::Side::SELL;
    }

    inline common::Order_OrderType toProto(Order::Type type) {
        if (type == Order::Type::LIMIT) return common::Order_OrderType_LIMIT;
        if (type == Order::Type::MARKET) return common::Order_OrderType_MARKET;
        return common::Order_OrderType_STOP;
    }

    inline Order::Type fromProto(common::Order_OrderType type) {
        if (type == common::Order_OrderType_LIMIT) return Order::Type::LIMIT;
        if (type == common::Order_OrderType_MARKET) return Order::Type::MARKET;
        return Order::Type::STOP;
    }
    inline google::protobuf::Timestamp toProtoTimestamp(std::chrono::system_clock::time_point tp) {
        google::protobuf::Timestamp ts;
        using namespace std::chrono;

        // Получаем длительность с начала эпохи
        auto duration = tp.time_since_epoch();

        // Извлекаем целые секунды
        auto seconds = duration_cast<std::chrono::seconds>(duration);
        ts.set_seconds(seconds.count());

        // Извлекаем остаток в наносекундах
        auto nanos = duration_cast<nanoseconds>(duration - seconds);
        ts.set_nanos(static_cast<int32_t>(nanos.count()));

        return ts;
    }
    // --- Complex Objects ---
    inline common::Order toProto(const Order& o) {
        common::Order po;
        po.set_id(o.id);
        po.set_user_id(o.user_id);
        po.set_instrument_id(o.instrument_id);
        po.set_side(toProto(o.side));
        po.set_type(toProto(o.type));
        po.set_status(toProto(o.status));
        *po.mutable_price() = toProto(o.price);
        *po.mutable_quantity() = toProto(o.quantity);
        *po.mutable_remaining_quantity() = toProto(o.remaining_quantity);

        *po.mutable_created_at() = toProtoTimestamp(o.created_at);
        return po;
    }

    inline common::Trade toProto(const Trade& t) {
        common::Trade pt;
        pt.set_id(t.id);
        pt.set_instrument_id(t.instrument_id);
        pt.set_buy_order_id(t.buy_order_id);
        pt.set_sell_order_id(t.sell_order_id);
        *pt.mutable_price() = toProto(t.price);
        *pt.mutable_quantity() = toProto(t.quantity);
        *pt.mutable_executed_at() = toProtoTimestamp(t.executed_at);
        return pt;
    }

    inline common::Candle toProto(const Candle& src) {
        common::Candle dst;

        // --- Timestamp ---
        *dst.mutable_time() = toProtoTimestamp(src.timestamp);

        // --- OHLC ---
        *dst.mutable_open()  = toProto(src.open);
        *dst.mutable_high()  = toProto(src.high);
        *dst.mutable_low()   = toProto(src.low);
        *dst.mutable_close() = toProto(src.close);

        // --- Volume ---
        *dst.mutable_volume() = toProto(src.volume);

        return dst;
    }

    inline AddPositionRequest fromProto(const admin::AddPositionRequest& src) {
        AddPositionRequest dst;

        // --- primitive ---
        dst.user_id = src.user_id();
        dst.instrument_id = src.instrument_id();

        // --- nested message ---
        dst.quantity = fromProto(src.quantity());

        return dst;
    }
}
