#include <grpcpp/grpcpp.h>

#include "../services/AuthService.h"
#include "../services/TradingService.h"
#include "../services/AccountService.h"
#include "../services/AdminService.h"
#include "core/core.h"
#include "storage/DatabaseManager.h"

int main() {
    // 1. Инициализация БД
    auto dbManager = DatabaseManager::instance();
    dbManager.init("192.168.100.10:3306", "root", "root", "trading_platform");
    auto conn = dbManager.getConnection();

    // 2. Создание репозиториев (все на одном коннекте)
    auto userRepo = std::make_shared<MySqlUserRepository>(conn);
    auto orderRepo = std::make_shared<MySqlOrderRepository>(conn);
    auto tradeRepo = std::make_shared<MySqlTradeRepository>(conn);
    auto accountRepo = std::make_shared<MySqlAccountRepository>(conn);
    auto matchingEngine = std::make_shared<MatchingEngine>();

    // 3. Создание Core (Фасад)
    auto core = std::make_shared<Core>(conn, userRepo, orderRepo, tradeRepo, accountRepo, matchingEngine);

    // 4. Настройка сервера
    grpc::ServerBuilder builder;
    builder.AddListeningPort("0.0.0.0:50051", grpc::InsecureServerCredentials());

    // 5. Регистрация сервисов
    AuthServiceImpl authService(core);
    TradingServiceImpl tradingService(core);
    AccountServiceImpl accountService(core);
    AdminServiceImpl adminService(core);

    builder.RegisterService(&authService);
    builder.RegisterService(&tradingService);
    builder.RegisterService(&accountService);
    builder.RegisterService(&adminService);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Gateway Server listening on 0.0.0.0:50051" << std::endl;
    server->Wait();

    return 0;
}