#include "account.grpc.pb.h"
#include "core/core.h"

class AccountServiceImpl final : public account::AccountService::Service
{
    std::shared_ptr<Core> m_core;

public:
    explicit AccountServiceImpl(std::shared_ptr<Core> core) : m_core(core) {}
};