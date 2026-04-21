#include "auth.grpc.pb.h"
#include "core/core.h"
#include "../utils/ProtoMapper.h"
#include <grpcpp/grpcpp.h>

class AuthServiceImpl final : public auth::AuthService::Service {
private:
    std::shared_ptr<Core> m_core;

    // Маппинг внутренних ошибок в gRPC статусы
    grpc::Status mapError(AuthError err) const {
        switch (err) {
            case AuthError::InvalidCredentials:
                return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid username or password");
            case AuthError::UserAlreadyExists:
                return grpc::Status(grpc::StatusCode::ALREADY_EXISTS, "User with this email/username already exists");
            case AuthError::UserNotFound:
                return grpc::Status(grpc::StatusCode::NOT_FOUND, "User not found");
            default:
                return grpc::Status(grpc::StatusCode::INTERNAL, "Internal authentication error");
        }
    }

public:
    explicit AuthServiceImpl(std::shared_ptr<Core> core) : m_core(core) {}

    // RPC: RegisterUser
    grpc::Status RegisterUser(grpc::ServerContext* context,
                             const auth::RegisterRequest* request,
                             auth::RegisterResponse* response) override
    {
        // Конвертируем запрос во внутреннюю команду (RegisterCommand)
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

    // RPC: AuthenticateUser (Login)
    grpc::Status AuthenticateUser(grpc::ServerContext* context,
                                 const auth::LoginRequest* request,
                                 auth::LoginResponse* response) override
    {
        LoginCommand cmd;
        cmd.username = request->username();
        cmd.password = request->password();

        auto result = m_core->login(cmd);

        if (!result) {
            return mapError(result.error());
        }

        // result содержит Token (string)
        response->set_token(*result);
        return grpc::Status::OK;
    }

    // RPC: GetUserById
    grpc::Status GetUserById(grpc::ServerContext* context,
                            const auth::GetUserRequest* request,
                            auth::User* response) override
    {
        // Здесь можно добавить проверку прав (например, только админ или сам юзер)
        // Но пока сделаем простую реализацию
        auto result = m_core->getUser(request->user_id());

        if (!result) {
            return mapError(result.error());
        }

        // Используем маппер для превращения нашей структуры User в Protobuf-сообщение
        *response = mapper::toProto(*result);
        return grpc::Status::OK;
    }
};