#include "admin.grpc.pb.h"
#include "core/core.h"

class AdminServiceImpl final : public admin::AdminService::Service
{
    std::shared_ptr<Core> m_core;

public:
    explicit AdminServiceImpl(std::shared_ptr<Core> core) : m_core(core) {}
};