#include "core/accountManager.h"
#include "core/TransactionGuard.h"

std::expected<Decimal, BalanceError>
AccountManager::getBalance(UserId id) const {
    // В реальной системе тут могла бы быть проверка, существует ли пользователь,
    // но репозиторий вернет 0, если данных нет, что для баланса допустимо.
    return m_accounts->getBalance(id);
}

std::expected<std::vector<Position>, BalanceError>
AccountManager::getPositions(UserId id) const {
    return m_accounts->getPositions(id);
}

std::expected<void, BalanceError>
AccountManager::deposit(const DepositCommand& cmd) {
    TransactionGuard tx(m_conn);
    if (cmd.amount <= Decimal{0, 0}) {
        return std::unexpected(BalanceError::InvalidAmount); // Нужно добавить в enum
    }

    uint64_t accId = m_accounts->getAccountIdByUserId(cmd.user_id);
    m_accounts->changeBalance(cmd.user_id, cmd.amount);

    m_accounts->addHistoryEntry(accId, cmd.amount, "DEPOSIT", std::nullopt);

    tx.commit();
    return {};
}

std::expected<void, BalanceError>
AccountManager::withdraw(const WithdrawCommand& cmd) {
    if (cmd.amount <= Decimal{0, 0}) {
        return std::unexpected(BalanceError::InvalidAmount);
    }

    uint64_t accId = m_accounts->getAccountIdByUserId(cmd.user_id);
    Decimal currentBalance = m_accounts->getBalance(cmd.user_id);
    if (currentBalance < cmd.amount) {
        return std::unexpected(BalanceError::InsuffucientMoney);
    }

    // Передаем отрицательную дельту для уменьшения баланса
    Decimal delta = cmd.amount;
    delta.units = -delta.units;
    delta.nanos = -delta.nanos;

    m_accounts->changeBalance(cmd.user_id, delta);
    m_accounts->addHistoryEntry(accId, cmd.amount, "DEPOSIT", std::nullopt);
    return {};
}

std::expected<std::vector<BalanceHistoryEntry>, BalanceError>
AccountManager::getBalanceHistory(UserId id) const {
    try {
        uint64_t accId = m_accounts->getAccountIdByUserId(id);
        return m_accounts->getHistory(accId);
    } catch (...) {
        return std::unexpected(BalanceError::InvalidUser);
    }
}