#pragma once

#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <random>

#include "core.h"

class TradingSimulator {
public:
    TradingSimulator(std::shared_ptr<Core> core, size_t botCount = 5);
    ~TradingSimulator();

    void start();
    void stop();

private:
    void setupBots();
    void simulationLoop();
    void generateRandomOrder();

    std::shared_ptr<Core> m_core;
    std::vector<UserId> m_botIds;
    std::vector<Instrument> m_instruments;

    std::atomic<bool> m_running{false};
    std::thread m_thread;

    std::mt19937 m_rng{std::random_device{}()};
};