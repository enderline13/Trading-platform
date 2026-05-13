#include "core/TradingSimulator.h"

#include <spdlog/spdlog.h>

TradingSimulator::TradingSimulator(std::shared_ptr<Core> core, size_t botCount)
    : m_core(std::move(core)), m_botCount(botCount)
{
    m_instruments = m_core->getAllInstruments();

    // Ищем инструмент с id = 1, иначе берём первый из списка
    bool found = false;
    for (const auto& instr : m_instruments) {
        if (instr.id == 1) {
            m_mainInstrument = instr;
            found = true;
            break;
        }
    }
    if (!found && !m_instruments.empty()) {
        m_mainInstrument = m_instruments.front();
        spdlog::warn("Instrument with id=1 not found, using id={}", m_mainInstrument.id);
    }

    setupBots();
}

void TradingSimulator::start() {
    if (m_running.exchange(true)) return; // уже запущен
    m_thread = std::thread(&TradingSimulator::simulationLoop, this);
}

void TradingSimulator::setupBots() {
    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    std::uniform_int_distribution<uint64_t> rnd_dist;
    uint64_t random_suffix = rnd_dist(m_rng);
    std::string prefix = "bot_" + std::to_string(timestamp) + "_" + std::to_string(random_suffix) + "_";

    for (size_t i = 0; i < m_botCount; ++i) {
        RegisterCommand regCmd;
        regCmd.username = prefix + std::to_string(i);
        regCmd.email    = prefix + std::to_string(i) + "@sim.local";
        regCmd.password = "sim";   // не используется для входа

        auto res = m_core->registerUser(regCmd);
        if (!res) {
            spdlog::error("Failed to register bot {}", regCmd.username);
            continue;
        }
        UserId botId = *res;
        m_botIds.push_back(botId);

        // Пополняем баланс
        DepositCommand depCmd{botId, Decimal(1'000'000, 0)};
        auto depRes = m_core->deposit(depCmd);
        if (!depRes) {
            spdlog::error("Failed to deposit for bot {}", botId);
            continue;
        }

        // Выдаём начальные позиции по ВСЕМ инструментам (чтобы боты могли торговать любыми)
        for (const auto& instr : m_instruments) {
            Decimal qty = instr.lot_size * Decimal(100, 0);  // 100 лотов
            AddPositionRequest posReq;
            posReq.user_id = botId;
            posReq.instrument_id = instr.id;
            posReq.quantity = qty;
            m_core->AddPosition(posReq);   // метод называется AddPosition (с большой A) – проверьте
        }

        spdlog::info("Bot {} created with id {}", regCmd.username, botId);
    }
    spdlog::info("Total {} bots ready", m_botIds.size());
}

void TradingSimulator::generateRandomOrder() {
    if (m_botIds.empty() || m_instruments.empty()) return;

    // Выбираем инструмент: 80% шанс основной, иначе случайный
    const Instrument* instr;
    if (m_instruments.size() == 1) {
        instr = &m_instruments[0];
    } else {
        std::bernoulli_distribution mainDist(0.8);
        if (mainDist(m_rng)) {
            instr = &m_mainInstrument;
        } else {
            std::uniform_int_distribution<size_t> instrDist(0, m_instruments.size() - 1);
            instr = &m_instruments[instrDist(m_rng)];
        }
    }

    // Бот
    std::uniform_int_distribution<size_t> botDist(0, m_botIds.size() - 1);
    UserId bot = m_botIds[botDist(m_rng)];

    // Сторона
    std::uniform_int_distribution<int> sideDist(0, 1);
    Order::Side side = static_cast<Order::Side>(sideDist(m_rng));

    // Тип ордера: 0-6 LIMIT, 7-8 MARKET, 9 STOP
    std::uniform_int_distribution<int> typeDist(0, 9);
    Order::Type type;
    int t = typeDist(m_rng);
    if (t <= 6) type = Order::Type::LIMIT;
    else if (t <= 8) type = Order::Type::MARKET;
    else type = Order::Type::STOP;

    // Получаем лучшие цены
    auto optBid = m_core->getBestBid(instr->id);
    auto optAsk = m_core->getBestAsk(instr->id);
    Decimal bestBid = optBid.value_or(m_fallbackPrice);
    Decimal bestAsk = optAsk.value_or(m_fallbackPrice);
    Decimal midPrice = (bestBid + bestAsk) / Decimal(2, 0); // не реализовано деление? Можно сложить и разделить скалярно.
    // Проще: midPrice = Decimal::fromDouble((decimalToDouble(bestBid) + decimalToDouble(bestAsk)) / 2.0);
    double bidD = decimalToDouble(bestBid);
    double askD = decimalToDouble(bestAsk);
    double mid = (bidD + askD) / 2.0;
    Decimal marketPrice = Decimal::fromDouble(mid);

    // Генерируем цену с отклонением до 5% от рыночной (для лимитных и стопов)
    std::uniform_real_distribution<double> priceFactor(0.95, 1.05);
    double factor = priceFactor(m_rng);
    Decimal rawPrice = Decimal::fromDouble(mid * factor);
    Decimal alignedPrice = rawPrice - (rawPrice % instr->tick_size); // выравнивание по тику
    if (alignedPrice <= Decimal{0,0}) alignedPrice = instr->tick_size;

    // Для стоп-ордеров цена может быть дальше от рынка
    if (type == Order::Type::STOP) {
        // стоп-цена: для BUY чуть выше рынка, для SELL чуть ниже
        if (side == Order::Side::BUY) {
            alignedPrice = Decimal::fromDouble(mid * 1.02); // 2% выше
        } else {
            alignedPrice = Decimal::fromDouble(mid * 0.98); // 2% ниже
        }
        alignedPrice = alignedPrice - (alignedPrice % instr->tick_size);
    }

    // Количество – случайное число лотов (1-10)
    std::uniform_int_distribution<int> lotCount(1, 10);
    int lots = lotCount(m_rng);
    Decimal qty = instr->lot_size * Decimal(lots, 0);

    // Формируем команду
    PlaceOrderCommand cmd;
    cmd.user_id = bot;
    cmd.instrument_id = instr->id;
    cmd.side = side;
    cmd.type = type;
    cmd.price = (type == Order::Type::MARKET) ? Decimal{0,0} : alignedPrice;
    cmd.quantity = qty;

    spdlog::debug("Simulator: Bot {} placing {} {} order qty={} price={}",
                  bot, static_cast<int>(side),
                  (type == Order::Type::LIMIT ? "LIMIT" : (type == Order::Type::MARKET ? "MARKET" : "STOP")),
                  qty.toString(), cmd.price.toString());

    auto result = m_core->placeOrder(cmd);
    if (!result) {
        spdlog::warn("Simulator: order rejected for bot {}: {}", bot, "error");
    }
}

void TradingSimulator::simulationLoop() {
    using namespace std::chrono;
    while (m_running) {
        generateRandomOrder();
        // Пауза 200-800 мс (можно варьировать для большей активности)
        std::this_thread::sleep_for(milliseconds(200 + m_rng() % 600));
    }
}