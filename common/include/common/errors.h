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

enum class AuthError {
    UserAlreadyExists,
    InvalidCredentials,
    UserNotFound,
    InvalidToken,
    InvalidInput
};

enum class BalanceError {
    InvalidUser,
    InvalidAmount,
    InsuffucientMoney
};

enum class TradingError {
    UserNotFound,
    InstrumentNotFound,
    InsufficientBalance,
    InsufficientPosition,
    InvalidOrder,

    MatchingFailed,
    OrderNotFound,
    Unauthorized
};