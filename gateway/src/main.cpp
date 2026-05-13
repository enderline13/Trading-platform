#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>

#include "../services/AuthService.h"
#include "../services/TradingService.h"
#include "../services/AccountService.h"
#include "../services/AdminService.h"
#include "../services/MarketService.h"
#include "core/core.h"
#include "core/TradingSimulator.h"
#include "storage/DatabaseManager.h"

int main() {
    auto dbManager = DatabaseManager::instance();
    dbManager.init("192.168.100.10:3306", "root", "root", "trading_platform");
    auto conn = dbManager.getConnection();

    auto userRepo = std::make_shared<MySqlUserRepository>(conn);
    auto orderRepo = std::make_shared<MySqlOrderRepository>(conn);
    auto tradeRepo = std::make_shared<MySqlTradeRepository>(conn);
    auto accountRepo = std::make_shared<MySqlAccountRepository>(conn);
    auto instrumentRepo = std::make_shared<MySqlInstrumentRepository>(conn);
    auto matchingEngine = std::make_shared<MatchingEngine>();
    auto marketDataManager = std::make_shared<MarketDataManager>();

    auto core = std::make_shared<Core>(conn, userRepo, orderRepo, tradeRepo, accountRepo, instrumentRepo, matchingEngine, marketDataManager);

    auto simulator = std::make_unique<TradingSimulator>(core, 5);

    grpc::ServerBuilder builder;
    builder.AddListeningPort("0.0.0.0:50051", grpc::InsecureServerCredentials());

    AuthServiceImpl authService(core);
    TradingServiceImpl tradingService(core);
    AccountServiceImpl accountService(core);
    AdminServiceImpl adminService(core);
    MarketServiceImpl marketService(core);

    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    builder.RegisterService(&authService);
    builder.RegisterService(&tradingService);
    builder.RegisterService(&accountService);
    builder.RegisterService(&adminService);
    builder.RegisterService(&marketService);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Gateway Server listening on 0.0.0.0:50051" << std::endl;
    simulator->start();
    server->Wait();

    return 0;
}