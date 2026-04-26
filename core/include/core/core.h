#pragma once

#include "accountManager.h"
#include "authManager.h"
#include "tradingCore.h"
#include "adminManager.h"
#include "storage/IAccountRepository.h"
#include "storage/IUserRepository.h"
#include "storage/ITradeRepository.h"
#include "storage/IOrderRepository.h"
#include "matching/matchingEngine.h"

class Core {
public:
    Core (
            std::shared_ptr<sql::Connection> conn,
            std::shared_ptr<IUserRepository> users,
            std::shared_ptr<IOrderRepository> orders,
            std::shared_ptr<ITradeRepository> trades,
            std::shared_ptr<IAccountRepository> accounts,
            std::shared_ptr<IInstrumentRepository> instruments,
            std::shared_ptr<MatchingEngine> matching
        )
            : m_conn(conn), auth(conn, users),
              trading(conn, orders, trades, accounts, instruments, matching),
              account(conn, accounts), admin(conn, instruments, accounts) {}

    std::expected<UserId, AuthError> registerUser(const RegisterCommand& cmd) const {
        return auth.registerUser(cmd);
    }
    std::expected<User, AuthError> getUser(const UserId id) const {
        return auth.getUser(id);
    }

    std::expected<std::vector<BalanceHistoryEntry>, BalanceError> getBalanceHistory(const UserId id) const {
        return account.getBalanceHistory(id);
    }

    std::expected<Token, AuthError> login(const LoginCommand& cmd) {
        return auth.login(cmd);
    }

    std::expected<User, AuthError> validateToken(const Token& token) const {
        return auth.validateToken(token);
    }

    // --- Trading API ---
    std::expected<OrderId, TradingError> placeOrder(const PlaceOrderCommand& cmd) const {
        return trading.placeOrder(cmd);
    }

    std::expected<void, TradingError> cancelOrder(const CancelOrderCommand& cmd) const {
        return trading.cancelOrder(cmd);
    }

    std::expected<std::vector<Order>, TradingError> getUserOrders(const GetOrdersQuery& query) const {
        return trading.getUserOrders(query);
    }

    std::expected<std::vector<Trade>, TradingError> getTradeHistory(const GetTradesQuery& query) const {
        return trading.getTradeHistory(query);
    }

    // --- Account API ---
    std::expected<Decimal, BalanceError> getBalance(const UserId id) const {
        return account.getBalance(id);
    }

    std::expected<std::vector<Position>, BalanceError> getPositions(const UserId id) const {
        return account.getPositions(id);
    }

    std::expected<void, BalanceError> deposit(const DepositCommand& cmd) const {
        return account.deposit(cmd);
    }

    std::expected<void, BalanceError> withdraw(const WithdrawCommand& cmd) const {
        return account.withdraw(cmd);
    }

    void addInstrument(const Instrument& i) {
        return admin.addInstrument(i);
    }

    void updateInstrument(const Instrument& i) {
        admin.updateInstrument(i);
    }

    void fundUser(const UserId id, const Decimal amount) {
       return admin.fundUser(id, amount);
    }

    admin::SystemStatus getSystemStatus() const {
      return admin.getSystemStatus();
    }

    std::vector<Instrument> getAllInstruments() const {
        return trading.getAllInstruments();
    }

    std::expected<Order, TradingError> getOrder(const OrderId orderId) const {
        return trading.getOrder(orderId);
    }

private:
    std::shared_ptr<sql::Connection> m_conn;

    AuthManager auth;
    TradingCore trading;
    AccountManager account;
    AdminManager admin;
};