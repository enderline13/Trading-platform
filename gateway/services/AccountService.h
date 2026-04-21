#include "account.grpc.pb.h"
#include "core/core.h"
#include "../utils/ProtoMapper.h"
#include <grpcpp/grpcpp.h>

class AccountServiceImpl final : public account::AccountService::Service {
private:
    std::shared_ptr<Core> m_core;

    // Вспомогательный метод для получения UserId из метаданных
    std::expected<User, grpc::Status> authenticate(grpc::ServerContext* context) const {
        auto metadata = context->client_metadata();
        auto it = metadata.find("authorization");
        if (it == metadata.end()) {
            return std::unexpected(grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Missing token"));
        }

        std::string token(it->second.data(), it->second.length());
        auto userOpt = m_core->validateToken(token);

        if (!userOpt) {
            return std::unexpected(grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid or expired token"));
        }
        return *userOpt;
    }

public:
    explicit AccountServiceImpl(std::shared_ptr<Core> core) : m_core(core) {}

    grpc::Status GetBalance(grpc::ServerContext* context,
                           const google::protobuf::Empty* request,
                           common::Decimal* response) override
    {
        auto auth = authenticate(context);
        if (!auth) return auth.error();

        auto balance = m_core->getBalance(auth->id);
        if (!balance) return grpc::Status(grpc::StatusCode::INTERNAL, "Could not fetch balance");

        *response = mapper::toProto(*balance);
        return grpc::Status::OK;
    }

    grpc::Status GetPositions(grpc::ServerContext* context,
                             const google::protobuf::Empty* request,
                             account::UserPositions* response) override
    {
        auto auth = authenticate(context);
        if (!auth) return auth.error();

        auto positions = m_core->getPositions(auth->id);
        if (!positions) return grpc::Status(grpc::StatusCode::INTERNAL, "Could not fetch positions");

        for (const auto& pos : *positions) {
            auto* p = response->add_positions();
            *p = mapper::toProto(pos);
        }
        return grpc::Status::OK;
    }

    grpc::Status GetBalanceHistory(grpc::ServerContext* context,
                                  const google::protobuf::Empty* request,
                                  account::BalanceHistory* response) override
    {
        auto auth = authenticate(context);
        if (!auth) return auth.error();

        auto history = m_core->getBalanceHistory(auth->id);
        if (!history) return grpc::Status(grpc::StatusCode::INTERNAL, "History unavailable");

        for (const auto& entry : *history) {
            // В твоем proto BalanceHistory содержит repeated common.Decimal
            // Мы записываем туда изменение баланса (amount)
            auto* b = response->add_balances();
            *b = mapper::toProto(entry.change_amount);
        }
        return grpc::Status::OK;
    }
};