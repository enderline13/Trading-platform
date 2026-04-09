#pragma once

#include <cstdint>
#include <compare>
#include <cmath>

struct Decimal {
    int64_t units = 0;
    int32_t nanos = 0;

    auto operator<=>(const Decimal&) const = default;
    static constexpr int32_t kNanoFactor = 1'000'000'000;

    void normalize() {
        units += nanos / kNanoFactor;
        nanos %= kNanoFactor;

        if (units > 0 && nanos < 0) {
            units--;
            nanos += kNanoFactor;
        } else if (units < 0 && nanos > 0) {
            units++;
            nanos -= kNanoFactor;
        }
    }

    Decimal& operator+=(const Decimal& other) {
        units += other.units;
        nanos += other.nanos;
        normalize();
        return *this;
    }

    friend Decimal operator+(Decimal lhs, const Decimal& rhs) {
        lhs += rhs;
        return lhs;
    }

    Decimal& operator-=(const Decimal& other) {
        units -= other.units;
        nanos -= other.nanos;
        normalize();
        return *this;
    }

    friend Decimal operator-(Decimal lhs, const Decimal& rhs) {
        lhs -= rhs;
        return lhs;
    }

    static Decimal fromDouble(double val) {
        // Извлекаем целую часть (с учетом знака)
        double intPart;
        double fractPart = std::modf(val, &intPart);

        // Округляем дробную часть до ближайшего нано-значения
        // Используем std::round, чтобы избежать ошибок точности (0.1 -> 0.099999999...)
        int64_t totalNanos = static_cast<int64_t>(std::round(fractPart * kNanoFactor));

        Decimal d;
        d.units = static_cast<int64_t>(intPart);
        d.nanos = static_cast<int32_t>(totalNanos);

        // Нормализация на случай, если округление дробной части
        // "перекинуло" значение в новую целую единицу (например, 0.9999999999)
        if (d.nanos >= kNanoFactor) {
            d.units += 1;
            d.nanos -= kNanoFactor;
        } else if (d.nanos <= -kNanoFactor) {
            d.units -= 1;
            d.nanos += kNanoFactor;
        }

        return d;
    }
};