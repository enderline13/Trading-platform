#pragma once

#include "account.grpc.pb.h"
#include "core/core.h"
#include "../utils/ProtoMapper.h"
#include "../utils/Authenticate.h"

class AccountServiceImpl final : public account::AccountService::Service {
private:
    std::shared_ptr<Core> m_core;

public:
    explicit AccountServiceImpl(std::shared_ptr<Core> core) : m_core(std::move(core)) {}

    grpc::Status GetBalance(grpc::ServerContext* context,
                           const google::protobuf::Empty* request,
                           common::Decimal* response) override
    {
        try {
            auto auth = authenticate(context, m_core);
            if (!auth) return auth.error();

            const auto balance = m_core->getBalance(auth->id);
            if (!balance) return {grpc::StatusCode::INTERNAL, "Could not fetch balance"};

            *response = mapper::toProto(*balance);
            return grpc::Status::OK;
        }
        catch (...) {
            return {grpc::StatusCode::INTERNAL, "Unknown internal error occurred"};
        }

    }

    grpc::Status GetPositions(grpc::ServerContext* context,
                             const google::protobuf::Empty* request,
                             account::UserPositions* response) override
    {
        try {
            auto auth = authenticate(context, m_core);
            if (!auth) return auth.error();

            const auto positions = m_core->getPositions(auth->id);
            if (!positions) return {grpc::StatusCode::INTERNAL, "Could not fetch positions"};

            std::ranges::for_each(positions.value(), [response](const auto& position) {
                auto* p = response->add_positions();
                *p = mapper::toProto(position);
            });

            return grpc::Status::OK;
        }
        catch (...) {
            return {grpc::StatusCode::INTERNAL, "Unknown internal error occurred"};
        }

    }

    grpc::Status GetBalanceHistory(grpc::ServerContext* context,
                                  const google::protobuf::Empty* request,
                                  account::BalanceHistory* response) override
    {
        try {
            auto auth = authenticate(context, m_core);
            if (!auth) return auth.error();

            const auto history = m_core->getBalanceHistory(auth->id);
            if (!history) return {grpc::StatusCode::INTERNAL, "Balance History unavailable"};

            for (const auto& entry : history.value()) {
                auto* protoEntry = response->add_entries();

                *protoEntry->mutable_change_amount() = mapper::toProto(entry.change_amount);
                protoEntry->set_reason(entry.reason);
                *protoEntry->mutable_timestamp() = mapper::toProtoTimestamp(entry.timestamp);
            }
            return grpc::Status::OK;
        }
        catch (...) {
            return {grpc::StatusCode::INTERNAL, "Unknown internal error occurred"};
        }

    }

    grpc::Status Deposit(grpc::ServerContext* context,
                    const account::DepositRequest* request,
                    google::protobuf::Empty* response) override
    {
        try {
            auto auth = authenticate(context, m_core);
            if (!auth) return auth.error();

            DepositCommand cmd;
            cmd.user_id = auth->id;
            cmd.amount = mapper::fromProto(request->amount());

            const auto result = m_core->deposit(cmd);
            return result ? grpc::Status::OK : grpc::Status(grpc::StatusCode::INTERNAL, "Deposit failed");
        }
        catch (...) {
            return {grpc::StatusCode::INTERNAL, "Unknown internal error occurred"};
        }
    }

    grpc::Status Withdraw(grpc::ServerContext* context,
                    const account::WithdrawRequest* request,
                    google::protobuf::Empty* response) override
    {
        try {
            auto auth = authenticate(context, m_core);
            if (!auth) return auth.error();

            WithdrawCommand cmd;
            cmd.user_id = auth->id;
            cmd.amount = mapper::fromProto(request->amount());

            const auto result = m_core->withdraw(cmd);

            return result ? grpc::Status::OK : grpc::Status(grpc::StatusCode::INTERNAL, "Withdrawal failed");
        }
        catch (...) {
            return {grpc::StatusCode::INTERNAL, "Unknown internal error occurred"};
        }
    }
};