#pragma once

#include <memory>

#include "admin.pb.h"
#include "storage/IInstrumentRepository.h"
#include "storage/IAccountRepository.h"

struct AddPositionRequest {
    uint64_t user_id = 0;
    uint64_t instrument_id = 0;

    Decimal quantity;
};

class AdminManager final {
public:
    AdminManager(std::shared_ptr<sql::Connection> conn, 
                 std::shared_ptr<IInstrumentRepository> instruments,
                 std::shared_ptr<IAccountRepository> accounts) 
        : m_conn(std::move(conn)), m_instruments(std::move(instruments)), m_accounts(std::move(accounts)) {}

    void addInstrument(const Instrument& i);
    void updateInstrument(const Instrument& i);
    void fundUser(UserId id, Decimal amount);
    admin::SystemStatus getSystemStatus() const;
    void setSystemState(bool running);
    void AddPosition(const AddPositionRequest& request) const;
private:
    std::shared_ptr<sql::Connection> m_conn;
    std::shared_ptr<IInstrumentRepository> m_instruments;
    std::shared_ptr<IAccountRepository> m_accounts;
};