#pragma once
#include "common.grpc.pb.h"
#include <google/protobuf/util/time_util.h>
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
        return d;
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

    inline auth::User toProto(const User& u) {
        auth::User pu;
        pu.set_id(u.id);
        pu.set_username(u.username);
        pu.set_email(u.email);
        return pu;
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
        *po.mutable_price() = toProto(o.price);
        *po.mutable_quantity() = toProto(o.quantity);
        *po.mutable_remaining_quantity() = toProto(o.remaining_quantity);

        // Преобразование времени
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


}