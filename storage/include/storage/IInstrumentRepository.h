#pragma once

#include <vector>
#include <memory>

#include "common/Instrument.h"
#include "common/Types.h"
#include "utils.h"

class IInstrumentRepository {
public:
    virtual ~IInstrumentRepository() = default;

    virtual void add(const Instrument& i) = 0;
    virtual void update(const Instrument& i) = 0;
    virtual std::vector<Instrument> getAll() = 0;
    virtual std::optional<Instrument> getById(InstrumentId id) = 0;
};

class MySqlInstrumentRepository final : public IInstrumentRepository {
private:
    std::shared_ptr<sql::Connection> m_conn;

public:
    MySqlInstrumentRepository(std::shared_ptr<sql::Connection> conn) : m_conn(std::move(conn)) {}

    void add(const Instrument& i) override {
        PrepStatementPtr pstmt(m_conn->prepareStatement(
            "INSERT INTO instruments (symbol, name, tick_size, lot_size, is_active) "
            "VALUES (?, ?, ?, ?, ?)"
        ));
        pstmt->setString(1, i.symbol);
        pstmt->setString(2, i.name);
        pstmt->setString(3, i.tick_size.toString());
        pstmt->setString(4, i.lot_size.toString());
        pstmt->setBoolean(5, i.is_active);
        pstmt->executeUpdate();
    }

    void update(const Instrument& i) override {
        PrepStatementPtr pstmt(m_conn->prepareStatement(
            "UPDATE instruments SET symbol = ?, name = ?, tick_size = ?, lot_size = ?, is_active = ? "
            "WHERE id = ?"
        ));

        pstmt->setString(1, i.symbol);
        pstmt->setString(2, i.name);

        // Используем конвертацию Decimal в строку для сохранения точности в DECIMAL(18,8)
        pstmt->setString(3, i.tick_size.toString());
        pstmt->setString(4, i.lot_size.toString());

        pstmt->setBoolean(5, i.is_active);
        pstmt->setUInt64(6, i.id);

        if (pstmt->executeUpdate() == 0) {
            throw std::runtime_error("Instrument not found with ID: " + std::to_string(i.id));
        }
    }

    std::vector<Instrument> getAll() override {
        std::vector<Instrument> result;
        StatementPtr stmt(m_conn->createStatement());
        ResultSetPtr res(stmt->executeQuery("SELECT * FROM instruments"));
        result.reserve(res->rowsCount());

        while (res->next()) {
            Instrument i;
            i.id = res->getUInt64("id");
            i.symbol = res->getString("symbol");
            i.name = res->getString("name");
            i.tick_size = decimalFromSql(res->getString("tick_size"));
            i.lot_size = decimalFromSql(res->getString("lot_size"));
            i.is_active = res->getBoolean("is_active");
            result.push_back(std::move(i));
        }

        return result;
    }

    std::optional<Instrument> getById(const InstrumentId id) override {
        PrepStatementPtr pstmt(m_conn->prepareStatement(
          "SELECT id, symbol, name, tick_size, lot_size, is_active FROM instruments WHERE id = ?"
        ));

        pstmt->setUInt64(1, id);
        ResultSetPtr res(pstmt->executeQuery());

        if (!res->next()) {
            return std::nullopt;
        }

        Instrument instr;
        instr.id = res->getInt64("id");
        instr.symbol = res->getString("symbol");
        instr.name = res->getString("name");
        instr.tick_size = decimalFromSql(res->getString("tick_size"));
        instr.lot_size  = decimalFromSql(res->getString("lot_size"));
        instr.is_active = res->getBoolean("is_active");

        return instr;
    }
};