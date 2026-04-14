#include "core/accountManager.h"

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
    if (cmd.amount <= Decimal{0, 0}) {
        return std::unexpected(BalanceError::InvalidAmount); // Нужно добавить в enum
    }

    m_accounts->changeBalance(cmd.user_id, cmd.amount);
    return {};
}

std::expected<void, BalanceError>
AccountManager::withdraw(const WithdrawCommand& cmd) {
    if (cmd.amount <= Decimal{0, 0}) {
        return std::unexpected(BalanceError::InvalidAmount);
    }

    Decimal currentBalance = m_accounts->getBalance(cmd.user_id);
    if (currentBalance < cmd.amount) {
        return std::unexpected(BalanceError::InsuffucientMoney);
    }

    // Передаем отрицательную дельту для уменьшения баланса
    Decimal delta = cmd.amount;
    delta.units = -delta.units;
    delta.nanos = -delta.nanos;

    m_accounts->changeBalance(cmd.user_id, delta);
    return {};
}