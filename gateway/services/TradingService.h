#include "trading.grpc.pb.h"
#include "core/core.h"
#include "../../common/include/common/ProtoMapper.h"
#include "../utils/Authenticate.h"

class TradingServiceImpl final : public trading::TradingService::Service {
private:
    std::shared_ptr<Core> m_core;

public:
    explicit TradingServiceImpl(std::shared_ptr<Core> core) : m_core(std::move(core)) {}

    grpc::Status PlaceOrder(grpc::ServerContext* context,
                           const trading::PlaceOrderRequest* request,
                           trading::PlaceOrderResponse* response) override
    {
        try {
            auto user = authenticate(context, m_core);
            if (!user) return user.error();
            const auto userId = user.value().id;

            PlaceOrderCommand cmd;
            cmd.user_id = userId;
            cmd.instrument_id = request->instrument_id();
            cmd.side = mapper::fromProto(request->side());
            cmd.type = mapper::fromProto(request->type());
            cmd.price = mapper::fromProto(request->price());
            cmd.quantity = mapper::fromProto(request->quantity());

            const auto result = m_core->placeOrder(cmd);
            if (!result) {
                return {grpc::StatusCode::FAILED_PRECONDITION, "Order rejected"};
            }

            response->set_order_id(result.value().id);
            response->set_status(mapper::toProto(result.value().status));
            return grpc::Status::OK;
        }
        catch (...) {
            return {grpc::StatusCode::INTERNAL, "Unknown internal error"};
        }
    }

    grpc::Status CancelOrder(grpc::ServerContext* context,
                            const trading::OrderID* request,
                            trading::CancelOrderResponse* response) override
    {
        try {
            auto user = authenticate(context, m_core);
            if (!user) return user.error();
            const auto userId = user.value().id;

            const CancelOrderCommand cmd{userId, request->order_id()};

            if (const auto result = m_core->cancelOrder(cmd); !result)
                return {grpc::StatusCode::NOT_FOUND, "Order not found or already filled"};

            response->set_status(common::Order_OrderStatus_CANCELED);
            return grpc::Status::OK;
        }
        catch (...) {
            return {grpc::StatusCode::INTERNAL, "Unknown internal error"};
        }
    }

    grpc::Status GetOrders(grpc::ServerContext* context,
                          const trading::GetOrdersRequest* request,
                          trading::Orders* response) override
    {
        try {
            auto user = authenticate(context, m_core);
            if (!user) return user.error();
            const auto userId = user.value().id;

            GetOrdersQuery query;
            query.user_id = userId;
            if (request->has_instrument_id()) query.instrument_id = request->instrument_id();
            if (request->has_status()) query.status = mapper::fromProto(request->status());

            const auto orders = m_core->getUserOrders(query);
            if (!orders) return {grpc::StatusCode::INTERNAL, "Fetch orders failed"};

            for (const auto& o : orders.value()) {
                *response->add_orders() = mapper::toProto(o);
            }
            return grpc::Status::OK;
        }
        catch (...) {
            return {grpc::StatusCode::INTERNAL, "Unknown internal error"};
        }
    }

    grpc::Status GetTradeHistory(grpc::ServerContext* context,
                                const trading::TradeHistoryRequest* request,
                                trading::Trades* response) override
    {
        try {
            auto user = authenticate(context, m_core);
            if (!user) return user.error();
            const auto userId = user.value().id;

            GetTradesQuery query;
            query.user_id = userId;
            if (request->has_instrument_id()) query.instrument_id = request->instrument_id();

            const auto trades = m_core->getTradeHistory(query);
            if (!trades) return {grpc::StatusCode::INTERNAL, "Fetch failed"};

            for (const auto& t : trades.value()) {
                *response->add_trades() = mapper::toProto(t);
            }
            return grpc::Status::OK;
        }
        catch (...) {
            return {grpc::StatusCode::INTERNAL, "Unknown internal error"};
        }
    }

    grpc::Status GetOrder(grpc::ServerContext* context,
                                         const trading::OrderID* request,
                                         common::Order* response) override {
        try {
            auto auth = authenticate(context, m_core);
            if (!auth) return auth.error();

            const auto order = m_core->getOrder(request->order_id());
            if (!order) return {grpc::StatusCode::NOT_FOUND, "Order not found"};

            if (order->user_id != auth->id) {
                return {grpc::StatusCode::PERMISSION_DENIED, "Access denied"};
            }

            *response = mapper::toProto(*order);
            return grpc::Status::OK;
        }
        catch (...) {
            return {grpc::StatusCode::INTERNAL, "Unknown internal error"};
        }
    }
};