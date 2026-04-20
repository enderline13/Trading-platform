#include "auth.grpc.pb.h"
#include "core/core.h"

class AuthServiceImpl final : public auth::AuthService::Service {
    std::shared_ptr<Core> m_core;

public:
    explicit AuthServiceImpl(std::shared_ptr<Core> core) : m_core(core) {}

    grpc::Status RegisterUser(grpc::ServerContext* context, 
                             const auth::RegisterRequest* request,
                             auth::RegisterResponse* response) override 
    {
        RegisterCommand cmd{request->username(), request->email(), request->password()};
        auto result = m_core->registerUser(cmd);

        if (result) {
            response->set_user_id(*result);
            return grpc::Status::OK;
        }
        return grpc::Status(grpc::StatusCode::ALREADY_EXISTS, "User registration failed");
    }

    grpc::Status AuthenticateUser(grpc::ServerContext* context,
                                 const auth::LoginRequest* request,
                                 auth::LoginResponse* response) override 
    {
        LoginCommand cmd{request->username(), request->password()};
        auto result = m_core->login(cmd);

        if (result) {
            response->set_token(*result);
            return grpc::Status::OK;
        }
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid credentials");
    }
};