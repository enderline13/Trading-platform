#pragma once

#include <vector>

#include "common/Types.h"
#include "common/Decimal.h"
#include "common/Position.h"

class AccountManager {
public:
    Decimal getBalance(UserId);
    std::vector<Position> getPositions(UserId);
};