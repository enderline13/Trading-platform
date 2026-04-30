#pragma once

#include <cstdint>
#include <chrono>

using UserId = uint64_t;
using OrderId = uint64_t;
using InstrumentId = uint64_t;
using AccountId = uint64_t;

using Timestamp = std::chrono::system_clock::time_point;

using Token = std::string;
using Token_view = std::string_view;

