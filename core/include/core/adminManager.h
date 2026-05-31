#pragma once

#include <memory>

#include "admin.pb.h"
#include "storage/IInstrumentRepository.h"
#include "storage/IAccountRepository.h"
#include "storage/IUserRepository.h"
#include "common/ProtoMapper.h"

class AdminManager final {
public:
    AdminManager(std::shared_ptr<sql::Connection> conn, 
                 std::shared_ptr<IInstrumentRepository> instruments,
                 std::shared_ptr<IAccountRepository> accounts, std::shared_ptr<IUserRepository> users)
        : m_conn(std::move(conn)), m_instruments(std::move(instruments)), m_accounts(std::move(accounts)), m_users(std::move(users)) {}

    void addInstrument(const Instrument& i) const;
    void updateInstrument(const Instrument& i) const;
    void fundUser(UserId id, Decimal amount) const;
    admin::SystemStatus getSystemStatus() const;
    void setSystemState(bool running) const;
    void AddPosition(const AddPositionRequest& request) const;
    void deleteInstrument(const InstrumentId id) const { m_instruments->remove(id); }
    std::vector<User> listUsers() const { return m_users->getAllUsers(); }
    void setUserRole(const UserId userId, const User::Role role) const { m_users->setUserRole(userId, role); }
    void setUserActive(const UserId userId, const bool active) const { m_users->setUserActive(userId, active); }
private:
    std::shared_ptr<sql::Connection> m_conn;
    std::shared_ptr<IInstrumentRepository> m_instruments;
    std::shared_ptr<IAccountRepository> m_accounts;
    std::shared_ptr<IUserRepository> m_users;
};