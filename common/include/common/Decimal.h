#pragma once

#include <cstdint>
#include <compare>
#include <cmath>
#include <string>
#include <format>

struct Decimal {
    int64_t units = 0;
    int32_t nanos = 0;

    auto operator<=>(const Decimal&) const = default;
    static constexpr int32_t kNanoFactor = 1'000'000'000;

    Decimal operator-() const {
        return {-units, -nanos};
    }

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

    Decimal operator*(const Decimal& other) const {
        // Переводим оба числа в общее количество нано-единиц
        // Используем __int128_t, чтобы результат умножения (до 10^18 * 10^18) не переполнился
        __int128_t v1 = static_cast<__int128_t>(units) * kNanoFactor + nanos;
        __int128_t v2 = static_cast<__int128_t>(other.units) * kNanoFactor + other.nanos;

        // При умножении фиксированных чисел результат имеет масштаб S^2
        // Формула: (a * b) / S
        __int128_t product = v1 * v2;
        __int128_t res_nanos = product / kNanoFactor;

        return {
            static_cast<int64_t>(res_nanos / kNanoFactor),
            static_cast<int32_t>(res_nanos % kNanoFactor)
        };
    }

    // Полезно также иметь оператор для умножения на целое число
    Decimal operator*(int64_t multiplier) const {
        __int128_t v = static_cast<__int128_t>(units) * kNanoFactor + nanos;
        __int128_t res = v * multiplier;

        return {
            static_cast<int64_t>(res / kNanoFactor),
            static_cast<int32_t>(res % kNanoFactor)
        };
    }

    Decimal operator/(const Decimal& other) const {
        __int128_t v1 = static_cast<__int128_t>(units) * kNanoFactor + nanos;
        __int128_t v2 = static_cast<__int128_t>(other.units) * kNanoFactor + other.nanos;

        // Чтобы не потерять точность при делении, сначала масштабируем числитель
        __int128_t result_total_nanos = (v1 * kNanoFactor) / v2;

        return {
            static_cast<int64_t>(result_total_nanos / kNanoFactor),
            static_cast<int32_t>(result_total_nanos % kNanoFactor)
        };
    }

    std::string toString() const {
        std::string res = std::format("{}.{:09}", units, std::abs(nanos));

        res.erase(res.find_last_not_of('0') + 1);
        if (res.back() == '.') res.pop_back();

        return res;
    }
};