#pragma once

#include "accountManager.h"
#include "authManager.h"
#include "tradingCore.h"
#include "storage/IAccountRepository.h"
#include "storage/IUserRepository.h"
#include "storage/ITradeRepository.h"
#include "storage/IOrderRepository.h"
#include "matching/matchingEngine.h"

class Core {
public:
    Core(
            std::shared_ptr<IUserRepository> users,
            std::shared_ptr<IOrderRepository> orders,
            std::shared_ptr<ITradeRepository> trades,
            std::shared_ptr<IAccountRepository> accounts,
            std::shared_ptr<MatchingEngine> matching
        )
            : auth(users),
              trading(orders, trades, accounts, matching),
              account(accounts) {}

    std::expected<UserId, AuthError> registerUser(const RegisterCommand& cmd) {
        return auth.registerUser(cmd);
    }

    std::expected<Token, AuthError> login(const LoginCommand& cmd) {
        return auth.login(cmd);
    }

    std::expected<User, AuthError> validateToken(const Token& token) const {
        return auth.validateToken(token);
    }

    // --- Trading API ---
    std::expected<OrderId, TradingError> placeOrder(const PlaceOrderCommand& cmd) {
        return trading.placeOrder(cmd);
    }

    std::expected<void, TradingError> cancelOrder(const CancelOrderCommand& cmd) {
        return trading.cancelOrder(cmd);
    }

    std::expected<std::vector<Order>, TradingError> getUserOrders(const GetOrdersQuery& query) const {
        return trading.getUserOrders(query);
    }

    std::expected<std::vector<Trade>, TradingError> getTradeHistory(const GetTradesQuery& query) const {
        return trading.getTradeHistory(query);
    }

    // --- Account API ---
    std::expected<Decimal, BalanceError> getBalance(UserId id) const {
        return account.getBalance(id);
    }

    std::expected<std::vector<Position>, BalanceError> getPositions(UserId id) const {
        return account.getPositions(id);
    }

    std::expected<void, BalanceError> deposit(const DepositCommand& cmd) {
        return account.deposit(cmd);
    }

    std::expected<void, BalanceError> withdraw(const WithdrawCommand& cmd) {
        return account.withdraw(cmd);
    }
private:
    AuthManager auth;
    TradingCore trading;
    AccountManager account;
};