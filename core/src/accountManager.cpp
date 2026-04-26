#include "core/accountManager.h"
#include "core/TransactionGuard.h"

std::expected<Decimal, BalanceError>
AccountManager::getBalance(const UserId id) const {
    auto balance = m_accounts->getBalance(id);
    if (!balance) return std::unexpected(BalanceError::InvalidUser);
    return balance.value();
}

std::expected<std::vector<Position>, BalanceError>
AccountManager::getPositions(const UserId id) const {
    return m_accounts->getPositions(id);
}

std::expected<void, BalanceError>
AccountManager::deposit(const DepositCommand& cmd) const {
    TransactionGuard tx(m_conn);
    if (cmd.amount <= Decimal{0, 0}) {
        return std::unexpected(BalanceError::InvalidAmount);
    }

    const AccountId accId = m_accounts->getAccountIdByUserId(cmd.user_id);
    m_accounts->changeBalance(cmd.user_id, cmd.amount);

    m_accounts->addHistoryEntry(accId, cmd.amount, "DEPOSIT", std::nullopt);

    tx.commit();
    return {};
}

std::expected<void, BalanceError>
AccountManager::withdraw(const WithdrawCommand& cmd) const {
    TransactionGuard tx(m_conn);
    if (cmd.amount <= Decimal{0, 0}) {
        return std::unexpected(BalanceError::InvalidAmount);
    }

    const AccountId accId = m_accounts->getAccountIdByUserId(cmd.user_id);
    const auto currentBalance = m_accounts->getBalance(cmd.user_id);
    if (!currentBalance) return std::unexpected(BalanceError::InvalidUser);
    if (currentBalance.value() < cmd.amount) return std::unexpected(BalanceError::InsufficientMoney);

    Decimal delta = cmd.amount;
    delta.units = -delta.units;
    delta.nanos = -delta.nanos;

    m_accounts->changeBalance(cmd.user_id, delta);
    m_accounts->addHistoryEntry(accId, cmd.amount, "WITHDRAWAL", std::nullopt);
    tx.commit();
    return {};
}

std::expected<std::vector<BalanceHistoryEntry>, BalanceError>
AccountManager::getBalanceHistory(const UserId id) const {
    AccountId accId = m_accounts->getAccountIdByUserId(id);
    return m_accounts->getHistory(accId);
}