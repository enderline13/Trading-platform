#pragma once

#include "admin.grpc.pb.h"
#include "core/core.h"
#include "common/ProtoMapper.h"

class AdminServiceImpl final : public admin::AdminService::Service {
private:
    std::shared_ptr<Core> m_core;
    std::chrono::system_clock::time_point m_startTime;

public:
    explicit AdminServiceImpl(std::shared_ptr<Core> core)
        : m_core(std::move(core)), m_startTime(std::chrono::system_clock::now()) {}

    grpc::Status AddInstrument(grpc::ServerContext* context,
                                           const common::Instrument* request,
                                           google::protobuf::Empty* response) override
    {
        try {
            const auto authResult = authenticate(context, m_core);
            if (!authResult) return {grpc::StatusCode::UNAUTHENTICATED, "Invalid token"};
            if (authResult.value().role != User::Role::ADMIN) {
                return {grpc::StatusCode::PERMISSION_DENIED, "Access denied: Admin role required"};
            }

            m_core->addInstrument(mapper::fromProto(*request));
            return grpc::Status::OK;
        }
        catch (...) {
            return {grpc::StatusCode::INTERNAL, "Unknown internal error"};
        }
    }

    grpc::Status UpdateInstrument(grpc::ServerContext* context,
                                                const common::Instrument* request,
                                                google::protobuf::Empty* response) override
    {
        try {
            const auto authResult = authenticate(context, m_core);
            if (!authResult) return {grpc::StatusCode::UNAUTHENTICATED, "Invalid token"};
            if (authResult->role != User::Role::ADMIN) {
                return {grpc::StatusCode::PERMISSION_DENIED, "Admin role required"};
            }
            if (request->id() == 0) {
                return {grpc::StatusCode::INVALID_ARGUMENT, "Instrument ID is required for update"};
            }

            m_core->updateInstrument(mapper::fromProto(*request));
            return grpc::Status::OK;
        }
        catch (...) {
            return {grpc::StatusCode::INTERNAL, "Unknown internal error"};
        }
    }

    grpc::Status FundUserAccount(grpc::ServerContext* context,
                                const admin::FundRequest* request,
                                google::protobuf::Empty* response) override
    {
        try {
            const auto authResult = authenticate(context, m_core);
            if (!authResult) return {grpc::StatusCode::UNAUTHENTICATED, "Invalid token"};
            if (authResult->role != User::Role::ADMIN) {
                return {grpc::StatusCode::PERMISSION_DENIED, "Admin role required"};
            }

            DepositCommand cmd;
            cmd.user_id = request->user_id();
            cmd.amount = mapper::fromProto(request->amount());

            if (const auto result = m_core->deposit(cmd); !result)
                return {grpc::StatusCode::INTERNAL, "Funding failed"};

            return grpc::Status::OK;
        }
        catch (...) {
            return {grpc::StatusCode::INTERNAL, "Unknown internal error"};
        }
    }

    grpc::Status GetSystemStatus(grpc::ServerContext* context,
                                const google::protobuf::Empty* request,
                                admin::SystemStatus* response) override
    {
        try {
            const auto authResult = authenticate(context, m_core);
            if (!authResult) return {grpc::StatusCode::UNAUTHENTICATED, "Invalid token"};
            if (authResult->role != User::Role::ADMIN) {
                return {grpc::StatusCode::PERMISSION_DENIED, "Admin role required"};
            }

            const auto status = m_core->getSystemStatus();

            response->set_is_running(status.is_running());
            response->set_active_orders_count(status.active_orders_count());
            response->set_total_users_count(status.total_users_count());
            response->set_server_version("1.0.0-rc1");

            const auto now = std::chrono::system_clock::now();
            const auto uptime = std::chrono::duration_cast<std::chrono::hours>(now - m_startTime).count();
            response->set_uptime(std::to_string(uptime) + " часов");

            return grpc::Status::OK;
        }
        catch (...) {
            return {grpc::StatusCode::INTERNAL, "Unknown internal error"};
        }
    }

    grpc::Status SetSystemState(
       grpc::ServerContext* context,
       const admin::SystemStateRequest* request,
       google::protobuf::Empty* response) override
    {
        try {
            const auto authResult = authenticate(context, m_core);
            if (!authResult) return {grpc::StatusCode::UNAUTHENTICATED, "Invalid token"};
            if (authResult->role != User::Role::ADMIN) {
                return {grpc::StatusCode::PERMISSION_DENIED, "Admin role required"};
            }

            m_core->setSystemState(request->run_trading());
            return grpc::Status::OK;
        }
        catch (...) {
            return {grpc::StatusCode::INTERNAL, "Unknown internal error"};
        }
    }

    grpc::Status AddPosition(
        grpc::ServerContext* context,
        const admin::AddPositionRequest* request,
        google::protobuf::Empty* response) override
    {
        try {
            const auto authResult = authenticate(context, m_core);
            if (!authResult) return {grpc::StatusCode::UNAUTHENTICATED, "Invalid token"};
            if (authResult->role != User::Role::ADMIN) {
                return {grpc::StatusCode::PERMISSION_DENIED, "Admin role required"};
            }

            m_core->AddPosition(mapper::fromProto(*request));
            return grpc::Status::OK;
        }
        catch (...) {
            return {grpc::StatusCode::INTERNAL, "Unknown internal error"};
        }
    }
};