#pragma once

#include <vector>
#include <expected>
#include "common/errors.h"

#include "common/Types.h"
#include "common/Decimal.h"
#include "common/Position.h"
#include "storage/IAccountRepository.h"

struct DepositCommand {
    UserId user_id = 0;
    Decimal amount;
};

struct WithdrawCommand {
    UserId user_id = 0;
    Decimal amount;
};

class AccountManager {
public:
    AccountManager(std::shared_ptr<IAccountRepository> accounts) : m_accounts(accounts) {}
    std::expected<Decimal, BalanceError> getBalance(UserId) const;
    std::expected<std::vector<Position>, BalanceError> getPositions(UserId) const;
    std::expected<void, BalanceError> deposit(const DepositCommand&);
    std::expected<void, BalanceError> withdraw(const WithdrawCommand&);

private:
    std::shared_ptr<IAccountRepository> m_accounts;
};