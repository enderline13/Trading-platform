#pragma once

#include <expected>
#include <string>

#include <grpcpp/grpcpp.h>

#include "core/core.h"
#include "common/User.h"
#include "common/Types.h"

inline std::expected<User, grpc::Status> authenticate(const grpc::ServerContext* context, const std::shared_ptr<Core>& core) {
    auto metadata = context->client_metadata();
    const auto it = metadata.find("authorization");
    if (it == metadata.end()) {
        return std::unexpected(grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Missing token"));
    }

    const Token_view token(it->second.data(), it->second.length());
    auto userOpt = core->validateToken(token);

    if (!userOpt) {
        return std::unexpected(grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid or expired token"));
    }
    return userOpt.value();
}
