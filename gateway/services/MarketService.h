#pragma once

#include "market.grpc.pb.h"
#include "core/core.h"
#include "../../common/include/common/ProtoMapper.h"
#include "../utils/ThreadSafeQueue.h"

class MarketServiceImpl final : public market::MarketService::Service {
private:
    std::shared_ptr<Core> m_core;
public:
    explicit MarketServiceImpl(std::shared_ptr<Core> core) : m_core(std::move(core)) {}

    grpc::Status ListInstruments(grpc::ServerContext* context,
                                 const google::protobuf::Empty* request,
                                 market::InstrumentsList* response) override {
        auto instruments = m_core->getAllInstruments();
        for (const auto& i : instruments) {
            *response->add_instruments() = mapper::toProto(i);
        }
        return grpc::Status::OK;
    }

    grpc::Status GetInstrumentDetails(grpc::ServerContext* context,
                                      const market::InstrumentId* request,
                                      common::Instrument* response) override {
        auto instruments = m_core->getAllInstruments();
        auto it = std::ranges::find_if(instruments.begin(), instruments.end(),
            [&](const auto& i) { return i.id == request->id(); });

        if (it == instruments.end()) {
            return {grpc::StatusCode::NOT_FOUND, "Instrument not found"};
        }
        *response = mapper::toProto(*it);
        return grpc::Status::OK;
    }

    grpc::Status GetOrderBook(grpc::ServerContext* context,
                              const market::InstrumentID* request,
                              market::OrderBook* response) override {
        auto instData = m_core->getMarketData().getInstrument(request->instrument_id());
        *response = instData->getBookSnapshot();
        return grpc::Status::OK;
    }

    grpc::Status GetRecentTrades(grpc::ServerContext* context,
                                 const market::InstrumentID* request,
                                 market::RecentTrades* response) override {
        auto instData = m_core->getMarketData().getInstrument(request->instrument_id());
        auto trades = instData->getTradesSnapshot();
        for (const auto& t : trades) {
            *response->add_trades() = t;
        }
        return grpc::Status::OK;
    }

    grpc::Status GetCandles(grpc::ServerContext* context,
                        const market::CandlesRequest* request,
                        market::Candles* response) override
    {
        auto instData = m_core->getMarketData().getInstrument(request->instrument_id());
        auto history = instData->getCandles();  // все свечи

        // Преобразуем begin/end в time_point, если заданы
        std::optional<std::chrono::system_clock::time_point> from, to;
        if (request->has_begin()) {
            from = std::chrono::system_clock::from_time_t(request->begin().seconds());
        }
        if (request->has_end()) {
            to = std::chrono::system_clock::from_time_t(request->end().seconds());
        }

        uint32_t limit = request->limit(); // 0 = без ограничения
        uint32_t count = 0;

        for (const auto& c : history) {
            // Фильтр по времени
            if (from && c.timestamp < *from) continue;
            if (to   && c.timestamp > *to)   continue;

            auto* proto_c = response->add_candles();

            *proto_c->mutable_time()   = mapper::toProtoTimestamp(c.timestamp);
            *proto_c->mutable_open()   = mapper::toProto(c.open);
            *proto_c->mutable_high()   = mapper::toProto(c.high);
            *proto_c->mutable_low()    = mapper::toProto(c.low);
            *proto_c->mutable_close()  = mapper::toProto(c.close);
            *proto_c->mutable_volume() = mapper::toProto(c.volume);

            if (limit > 0 && ++count >= limit) break;
        }
        return grpc::Status::OK;
    }

    grpc::Status StreamOrderBook(grpc::ServerContext* context,
                                 const market::InstrumentID* request,
                                 grpc::ServerWriter<market::OrderBookUpdate>* writer) override {
        auto instData = m_core->getMarketData().getInstrument(request->instrument_id());
        auto queue = std::make_shared<ThreadSafeQueue<market::OrderBook>>();

        uint64_t subId = instData->subscribeBook([queue](const market::OrderBook& book) {
            queue->push(book);
        });

        auto cleanup = [&]() { instData->unsubscribeBook(subId); };

        spdlog::info("User subscribed to Book stream: {}", request->instrument_id());

        market::OrderBook book;
        while (!context->IsCancelled()) {
            if (queue->pop(book, std::chrono::milliseconds(500))) {
                market::OrderBookUpdate update;
                *update.mutable_bids() = book.bids();
                *update.mutable_asks() = book.asks();

                if (!writer->Write(update)) break; // Клиент отключился
            }
        }

        cleanup();
        return grpc::Status::OK;
    }

    grpc::Status StreamTrades(grpc::ServerContext* context,
                              const market::InstrumentID* request,
                              grpc::ServerWriter<common::Trade>* writer) override {
        auto instData = m_core->getMarketData().getInstrument(request->instrument_id());
        auto queue = std::make_shared<ThreadSafeQueue<common::Trade>>();

        uint64_t subId = instData->subscribeTrades([queue](const common::Trade& t) {
            queue->push(t);
        });

        common::Trade trade;
        while (!context->IsCancelled()) {
            if (queue->pop(trade, std::chrono::milliseconds(500))) {
                if (!writer->Write(trade)) break;
            }
        }

        instData->unsubscribeTrades(subId);
        return grpc::Status::OK;
    }

    grpc::Status StreamCandles(grpc::ServerContext* context,
                               const market::CandlesRequest* request,
                               grpc::ServerWriter<common::Candle>* writer) override {
        auto instData = m_core->getMarketData().getInstrument(request->instrument_id());

        auto queue = std::make_shared<ThreadSafeQueue<common::Candle>>();

        uint64_t subId = instData->subscribeCandles([queue](const common::Candle& candle) {
            queue->push(candle);
        });

        spdlog::info("New Candle Stream subscriber for instrument {}", request->instrument_id());

        common::Candle candleUpdate;
        while (!context->IsCancelled()) {
            if (queue->pop(candleUpdate, std::chrono::milliseconds(1000))) {
                if (!writer->Write(candleUpdate)) {
                    break;
                }
            }
        }

        instData->unsubscribeCandles(subId);
        spdlog::info("Candle Stream subscriber disconnected: {}", request->instrument_id());

        return grpc::Status::OK;
    }
};