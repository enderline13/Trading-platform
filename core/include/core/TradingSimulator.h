#pragma once
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <random>
#include "core.h"
#include "common/Decimal.h"

class TradingSimulator {
public:
    explicit TradingSimulator(std::shared_ptr<Core> core, size_t botCount = 5);

    void start();

private:
    void setupBots();
    void simulationLoop();
    void generateRandomOrder();

    std::shared_ptr<Core> m_core;
    std::vector<UserId> m_botIds;
    std::vector<Instrument> m_instruments;
    Instrument m_mainInstrument;  // инструмент 1 (или первый доступный)
    Decimal m_fallbackPrice{50000, 0};  // если стакан пуст

    std::atomic<bool> m_running{false};
    std::thread m_thread;

    size_t m_botCount;

    std::mt19937 m_rng{std::random_device{}()};
};