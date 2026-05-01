#include "auth.grpc.pb.h"
#include "core/core.h"
#include "../utils/ProtoMapper.h"
#include "../utils/Authenticate.h"

class AuthServiceImpl final : public auth::AuthService::Service {
private:
    std::shared_ptr<Core> m_core;

    static grpc::Status mapError(const AuthError err) {
        switch (err) {
            case AuthError::InvalidCredentials:
                return {grpc::StatusCode::UNAUTHENTICATED, "Invalid username or password"};
            case AuthError::UserAlreadyExists:
                return {grpc::StatusCode::ALREADY_EXISTS, "User with this email/username already exists"};
            case AuthError::UserNotFound:
                return {grpc::StatusCode::NOT_FOUND, "User not found"};
            default:
                return {grpc::StatusCode::INTERNAL, "Internal authentication error"};
        }
    }

public:
    explicit AuthServiceImpl(std::shared_ptr<Core> core) : m_core(std::move(core)) {}

    grpc::Status RegisterUser(grpc::ServerContext* context,
                             const auth::RegisterRequest* request,
                             auth::RegisterResponse* response) override
    {
        try {
            RegisterCommand cmd;
            cmd.username = request->username();
            cmd.email = request->email();
            cmd.password = request->password();

            auto result = m_core->registerUser(cmd);

            if (!result) {
                return mapError(result.error());
            }

            response->set_user_id(*result);
            return grpc::Status::OK;
        }
        catch (...) {
            return {grpc::StatusCode::INTERNAL, "Unknown internal error occurred"};
        }
    }

    grpc::Status AuthenticateUser(grpc::ServerContext* context,
                                 const auth::LoginRequest* request,
                                 auth::LoginResponse* response) override
    {
        try {
            LoginCommand cmd;
            cmd.username = request->username();
            cmd.password = request->password();

            auto result = m_core->login(cmd);

            if (!result) {
                return mapError(result.error());
            }

            response->set_token(*result);
            return grpc::Status::OK;
        }
        catch (...) {
            return {grpc::StatusCode::INTERNAL, "Unknown internal error occurred"};
        }
    }

    grpc::Status GetUserById(grpc::ServerContext* context,
                            const auth::GetUserRequest* request,
                            auth::User* response) override
    {
        try {
            auto result = m_core->getUser(request->user_id());

            if (!result) {
                return mapError(result.error());
            }

            *response = mapper::toProto(*result);
            return grpc::Status::OK;
        }
        catch (...) {
            return {grpc::StatusCode::INTERNAL, "Unknown internal error occurred"};
        }
    }

    grpc::Status GetCurrentUserId(grpc::ServerContext* context,
    const google::protobuf::Empty* request,
           auth::User* response) override {
        try {
            auto result = authenticate(context, m_core);

            if (!result) return result.error();

            *response = mapper::toProto(*result);
            return grpc::Status::OK;
        }
        catch (...) {
            return {grpc::StatusCode::INTERNAL, "Unknown internal error occurred"};
        }
    }
};