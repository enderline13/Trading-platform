#include "trading.grpc.pb.h"
#include "core/core.h"

class TradingServiceImpl final : public trading::TradingService::Service
{
    std::shared_ptr<Core> m_core;

public:
    explicit TradingServiceImpl(std::shared_ptr<Core> core) : m_core(core) {}
};