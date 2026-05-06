#include "core/accountManager.h"

#include <spdlog/spdlog.h>

#include "core/TransactionGuard.h"

std::expected<Decimal, BalanceError>
AccountManager::getBalance(const UserId id) const {
    spdlog::info("Getting balance for user {}", id);
    auto balance = m_accounts->getBalance(id);
    if (!balance) {
        spdlog::error("Could not get balance for user {}", id);
        return std::unexpected(BalanceError::InvalidUser);
    }
    return balance.value();
}

std::expected<std::vector<Position>, BalanceError>
AccountManager::getPositions(const UserId id) const {
    spdlog::info("Getting positions balance for user {}", id);
    return m_accounts->getPositions(id);
}

std::expected<void, BalanceError>
AccountManager::deposit(const DepositCommand& cmd) const {
    spdlog::info("Depositing {} money for user {}", cmd.amount.toString(), cmd.user_id);
    TransactionGuard tx(m_conn);
    if (cmd.amount <= Decimal{0, 0}) {
        spdlog::error("Trying to deposit < 0");
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
    spdlog::info("Withdrawing {} money from user {}", cmd.amount.toString(), cmd.user_id);
    TransactionGuard tx(m_conn);
    if (cmd.amount <= Decimal{0, 0}) {
        spdlog::error("Trying to withdraw < 0");
        return std::unexpected(BalanceError::InvalidAmount);
    }

    const AccountId accId = m_accounts->getAccountIdByUserId(cmd.user_id);
    const auto currentBalance = m_accounts->getBalance(cmd.user_id);
    if (!currentBalance) {
        spdlog::info("Could not get balance for user {}", cmd.user_id);
        return std::unexpected(BalanceError::InvalidUser);
    }
    if (currentBalance.value() < cmd.amount) {
        spdlog::error("Not enough money to withdraw");
        return std::unexpected(BalanceError::InsufficientMoney);
    }
    const Decimal delta = -cmd.amount;

    m_accounts->changeBalance(cmd.user_id, delta);
    m_accounts->addHistoryEntry(accId, delta, "WITHDRAWAL", std::nullopt);
    tx.commit();
    return {};
}

std::expected<std::vector<BalanceHistoryEntry>, BalanceError>
AccountManager::getBalanceHistory(const UserId id) const {
    const AccountId accId = m_accounts->getAccountIdByUserId(id);
    return m_accounts->getHistory(accId);
}