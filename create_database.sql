CREATE DATABASE trading_platform;

USE trading_platform;

CREATE TABLE users (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    email VARCHAR(255) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    role ENUM('USER', 'ADMIN') NOT NULL DEFAULT 'USER',
    is_active BOOLEAN NOT NULL DEFAULT TRUE,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE accounts (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL UNIQUE,
    balance_cash DECIMAL(18,8) NOT NULL DEFAULT 0,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(id)
);

CREATE TABLE instruments (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    symbol VARCHAR(32) NOT NULL UNIQUE,
    name VARCHAR(255) NOT NULL,
    tick_size DECIMAL(18,8) NOT NULL,
    lot_size DECIMAL(18,8) NOT NULL,
    is_active BOOLEAN NOT NULL DEFAULT TRUE,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE orders (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL,
    instrument_id BIGINT NOT NULL,
    type ENUM('LIMIT','MARKET','STOP') NOT NULL,
    side ENUM('BUY','SELL') NOT NULL,
    price DECIMAL(18,8) NULL,
    stop_price DECIMAL(18,8) NULL,
    quantity DECIMAL(18,8) NOT NULL,
    remaining_quantity DECIMAL(18,8) NOT NULL,
    status ENUM(
        'NEW',
        'PARTIALLY_FILLED',
        'FILLED',
        'CANCELED',
        'REJECTED'
    ) NOT NULL DEFAULT 'NEW',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,

    FOREIGN KEY (user_id) REFERENCES users(id),
    FOREIGN KEY (instrument_id) REFERENCES instruments(id),

    INDEX idx_instrument_status (instrument_id, status),
    INDEX idx_user (user_id)
);

CREATE TABLE trades (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    instrument_id BIGINT NOT NULL,
    buy_order_id BIGINT NOT NULL,
    sell_order_id BIGINT NOT NULL,
    price DECIMAL(18,8) NOT NULL,
    quantity DECIMAL(18,8) NOT NULL,
    fee_buy DECIMAL(18,8) NOT NULL DEFAULT 0,
    fee_sell DECIMAL(18,8) NOT NULL DEFAULT 0,
    executed_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,

    FOREIGN KEY (instrument_id) REFERENCES instruments(id),
    FOREIGN KEY (buy_order_id) REFERENCES orders(id),
    FOREIGN KEY (sell_order_id) REFERENCES orders(id),

    INDEX idx_instrument_time (instrument_id, executed_at)
);

CREATE TABLE positions (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    account_id BIGINT NOT NULL,
    instrument_id BIGINT NOT NULL,
    quantity DECIMAL(18,8) NOT NULL DEFAULT 0,
    average_price DECIMAL(18,8) NOT NULL DEFAULT 0,

    UNIQUE (account_id, instrument_id),

    FOREIGN KEY (account_id) REFERENCES accounts(id),
    FOREIGN KEY (instrument_id) REFERENCES instruments(id)
);

CREATE TABLE balance_history (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    account_id BIGINT NOT NULL,
    change_amount DECIMAL(18,8) NOT NULL,
    reason ENUM('TRADE','DEPOSIT','WITHDRAWAL','FEE') NOT NULL,
    reference_id BIGINT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,

    FOREIGN KEY (account_id) REFERENCES accounts(id)
);

CREATE TABLE system_state (
    id TINYINT PRIMARY KEY,
    trading_status ENUM('RUNNING','STOPPED') NOT NULL,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);

INSERT INTO system_state (id, trading_status) VALUES (1, 'RUNNING');

DELIMITER //

CREATE TRIGGER after_user_insert
AFTER INSERT ON trading_platform.users
FOR EACH ROW
BEGIN
    INSERT INTO trading_platform.accounts (user_id, balance_cash)
    VALUES (NEW.id, '0.00000000');
END //

DELIMITER ;