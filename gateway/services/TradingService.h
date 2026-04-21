#include "trading.grpc.pb.h"
#include "core/core.h"
#include "../utils/ProtoMapper.h"

class TradingServiceImpl final : public trading::TradingService::Service {
private:
    std::shared_ptr<Core> m_core;

    // Вспомогательный метод для получения UserId (аналогично AccountService)
    std::expected<UserId, grpc::Status> getAuthorizedUserId(grpc::ServerContext* context) const {
        auto metadata = context->client_metadata();
        auto it = metadata.find("authorization");
        if (it == metadata.end()) return std::unexpected(grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "No token"));

        auto user = m_core->validateToken(std::string(it->second.data(), it->second.length()));
        if (!user) return std::unexpected(grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Session expired"));
        return user->id;
    }

public:
    explicit TradingServiceImpl(std::shared_ptr<Core> core) : m_core(core) {}

    // 1. Размещение ордера
    grpc::Status PlaceOrder(grpc::ServerContext* context,
                           const trading::PlaceOrderRequest* request,
                           trading::PlaceOrderResponse* response) override
    {
        auto userId = getAuthorizedUserId(context);
        if (!userId) return userId.error();

        PlaceOrderCommand cmd;
        cmd.user_id = *userId;
        cmd.instrument_id = request->instrument_id();
        cmd.side = mapper::fromProto(request->side());
        cmd.type = mapper::fromProto(request->type());
        cmd.price = mapper::fromProto(request->price());
        cmd.quantity = mapper::fromProto(request->quantity());

        auto result = m_core->placeOrder(cmd);
        if (!result) {
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Order rejected");
        }

        response->set_order_id(*result);
        response->set_status(common::Order_OrderStatus_NEW);
        return grpc::Status::OK;
    }

    // 2. Отмена ордера
    grpc::Status CancelOrder(grpc::ServerContext* context,
                            const trading::OrderID* request,
                            trading::CancelOrderResponse* response) override
    {
        auto userId = getAuthorizedUserId(context);
        if (!userId) return userId.error();

        CancelOrderCommand cmd{*userId, request->order_id()};
        auto result = m_core->cancelOrder(cmd);

        if (!result) return grpc::Status(grpc::StatusCode::NOT_FOUND, "Order not found or already filled");

        response->set_status(common::Order_OrderStatus_CANCELED);
        return grpc::Status::OK;
    }

    // 3. Получение списка ордеров пользователя
    grpc::Status GetOrders(grpc::ServerContext* context,
                          const trading::GetOrdersRequest* request,
                          trading::Orders* response) override
    {
        auto userId = getAuthorizedUserId(context);
        if (!userId) return userId.error();

        GetOrdersQuery query;
        query.user_id = *userId;
        if (request->has_instrument_id()) query.instrument_id = request->instrument_id();
        // Здесь можно добавить фильтрацию по статусу, если Core её поддерживает

        auto orders = m_core->getUserOrders(query);
        if (!orders) return grpc::Status(grpc::StatusCode::INTERNAL, "Fetch failed");

        for (const auto& o : *orders) {
            *response->add_orders() = mapper::toProto(o);
        }
        return grpc::Status::OK;
    }

    // 4. История сделок пользователя
    grpc::Status GetTradeHistory(grpc::ServerContext* context,
                                const google::protobuf::Empty* request,
                                trading::Trades* response) override
    {
        auto userId = getAuthorizedUserId(context);
        if (!userId) return userId.error();

        GetTradesQuery query;
        query.user_id = *userId;

        auto trades = m_core->getTradeHistory(query);
        if (!trades) return grpc::Status(grpc::StatusCode::INTERNAL, "Fetch failed");

        for (const auto& t : *trades) {
            *response->add_trades() = mapper::toProto(t);
        }
        return grpc::Status::OK;
    }
};