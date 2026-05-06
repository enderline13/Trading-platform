#include "core/TradingSimulator.h"

#include <random>
#include <chrono>

#include <spdlog/spdlog.h>

TradingSimulator::TradingSimulator(std::shared_ptr<Core> core, size_t botCount)
    : m_core(std::move(core))
{
    // Получаем список инструментов
    m_instruments = m_core->getAllInstruments();

    // Создаём ботов
    for (size_t i = 0; i < botCount; ++i) {
        RegisterCommand regCmd;
        regCmd.username = "bot_" + std::to_string(i);
        regCmd.email = "bot" + std::to_string(i) + "@sim.local";
        regCmd.password = "sim"; // не используется для входа

        auto res = m_core->registerUser(regCmd);
        if (res) {
            UserId botId = *res;
            m_botIds.push_back(botId);

            // Пополняем баланс
            DepositCommand depCmd{botId, Decimal(1'000'000, 0)};
            m_core->deposit(depCmd);

            // Выдаём начальные позиции по каждому инструменту
            for (const auto& instr : m_instruments) {
                // Количество кратное лоту (например, 100 лотов)
                Decimal qty = instr.lot_size * Decimal(100, 0);
                AddPositionRequest posReq;
                posReq.user_id = botId;
                posReq.instrument_id = instr.id;
                posReq.quantity = qty;
                m_core->AddPosition(posReq);
            }

            spdlog::info("Bot {} created with id {}", regCmd.username, botId);
        } else {
            spdlog::error("Failed to create bot {}", regCmd.username);
        }
    }
}

TradingSimulator::~TradingSimulator() {
    stop();
}

void TradingSimulator::start() {
    if (m_running.exchange(true)) return; // уже запущен
    m_thread = std::thread(&TradingSimulator::simulationLoop, this);
}

void TradingSimulator::stop() {
    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void TradingSimulator::simulationLoop() {
    using namespace std::chrono;
    while (m_running) {
        generateRandomOrder();
        // Пауза 200-800 мс
        std::this_thread::sleep_for(milliseconds(200 + m_rng() % 600));
    }
}

void TradingSimulator::generateRandomOrder() {
    if (m_botIds.empty() || m_instruments.empty()) return;

    std::uniform_int_distribution<size_t> botDist(0, m_botIds.size() - 1);
    std::uniform_int_distribution<size_t> instrDist(0, m_instruments.size() - 1);
    std::uniform_int_distribution<int> sideDist(0, 1);
    std::uniform_int_distribution<int> typeDist(0, 9); // 0-6 LIMIT, 7-8 MARKET, 9 STOP

    UserId bot = m_botIds[botDist(m_rng)];
    const Instrument& instr = m_instruments[instrDist(m_rng)];
    Order::Side side = static_cast<Order::Side>(sideDist(m_rng));

    PlaceOrderCommand cmd;
    cmd.user_id = bot;
    cmd.instrument_id = instr.id;
    cmd.side = side;

    int typeRand = typeDist(m_rng);
    if (typeRand <= 6) {
        cmd.type = Order::Type::LIMIT;
    } else if (typeRand <= 8) {
        cmd.type = Order::Type::MARKET;
    } else {
        cmd.type = Order::Type::STOP;
    }

    // Получаем рыночную цену для ориентира
    auto optBestAsk = m_core->getBestAsk(instr.id); // Надо добавить метод в Core или использовать MatchingEngine напрямую
    // Допустим, у Core есть метод getBestAsk, который вызывает matchingEngine->getBestAsk(instrId)
    // Если нет, можно получить через MarketDataManager::getBestAsk, но проще добавить в Core.
    // Предположим, что такой метод есть.
    Decimal marketPrice(100, 0); // fallback
    if (optBestAsk.has_value()) {
        marketPrice = optBestAsk.value();
    }

    // Генерируем цену с отклонением до 5%
    std::uniform_real_distribution<double> priceFactor(0.95, 1.05);
    double factor = priceFactor(m_rng);
    // Приводим цену к тику
    Decimal rawPrice = marketPrice * Decimal::fromDouble(factor);
    // Выравниваем по tick_size
    Decimal remainder = rawPrice % instr.tick_size;
    Decimal alignedPrice = rawPrice - remainder;
    if (alignedPrice <= Decimal(0,0)) alignedPrice = instr.tick_size; // минимальный шаг

    cmd.price = alignedPrice;

    // Количество – случайное число лотов (1-10)
    std::uniform_int_distribution<int> lotCount(1, 10);
    int lots = lotCount(m_rng);
    cmd.quantity = instr.lot_size * Decimal(lots, 0);

    spdlog::debug("Simulator: Bot {} placing {} {} order qty={} price={}",
                  bot, static_cast<int>(cmd.side), 
                  (cmd.type == Order::Type::LIMIT ? "LIMIT" : (cmd.type == Order::Type::MARKET ? "MARKET" : "STOP")),
                  cmd.quantity.toString(), cmd.price.toString());

    auto result = m_core->placeOrder(cmd);
    if (!result) {
        spdlog::warn("Simulator: order rejected for bot {}: {}", bot, "some error");
    }
}