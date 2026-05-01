#include "core/adminManager.h"

void AdminManager::addInstrument(const Instrument& i) { m_instruments->add(i); }

void AdminManager::fundUser(const UserId id, const Decimal amount) {
    const auto accId = m_accounts->getAccountIdByUserId(id);
    m_accounts->updateBalance(accId, amount);
}

admin::SystemStatus AdminManager::getSystemStatus() const {
    admin::SystemStatus status;

    auto countQuery = [&](const std::string& table) {
        auto stmt = m_conn->createStatement();
        auto res = stmt->executeQuery("SELECT COUNT(*) FROM " + table);
        return res->next() ? res->getUInt64(1) : 0;
    };
    status.set_total_users_count(countQuery("users"));
    status.set_active_orders_count(countQuery("orders"));

    return status;
}

void AdminManager::updateInstrument(const Instrument& i) {
    m_instruments->update(i);
}

void AdminManager::setSystemState(bool running) {
    m_accounts->setSystemStatus(running);
    // Здесь можно добавить широковещательное уведомление через gRPC Stream в будущем
}

void AdminManager::AddPosition(const AddPositionRequest& request) const {
    m_accounts->addPosition(request.user_id, request.instrument_id, request.quantity);
}