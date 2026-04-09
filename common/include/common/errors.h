#pragma once

enum class MatchingError {
    OrderNotFound,
    AlreadyFilled,
    EmptyOrder,
    BrokenOrder,
    InstrumentMismatch,
    OrderAlreadyFilled,
    OrderAlreadyCanceled,
    InvalidPrice,
    InvalidQuantity,
    InvalidId,
    InvalidStatus,
    InstrumentNotFound
};

enum class TradingError {
    InsufficientFunds,
    InstrumentNotFound,
    TradingStopped,
    InvalidOrder
};

enum class AuthError {
    UserAlreadyExists,
    InvalidCredentials,
    UserNotFound
};