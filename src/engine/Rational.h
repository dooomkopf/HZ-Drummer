#pragma once

#include <cstdint>
#include <numeric>

namespace hz
{

// Exact fraction for musical positions (in quarters). Always reduced, denominator > 0.
// Values stay small (bars * grid resolution); overflows are prevented by limits on load.
struct Rational
{
    std::int64_t num = 0;
    std::int64_t den = 1;

    constexpr Rational() = default;
    constexpr Rational (std::int64_t n, std::int64_t d = 1) : num (n), den (d) { normalise(); }

    constexpr void normalise()
    {
        if (den < 0) { num = -num; den = -den; }
        if (den == 0) { num = 0; den = 1; return; }
        const auto g = std::gcd (num < 0 ? -num : num, den);
        if (g > 1) { num /= g; den /= g; }
    }

    constexpr double toDouble() const { return (double) num / (double) den; }

    friend constexpr Rational operator+ (Rational a, Rational b) { return { a.num * b.den + b.num * a.den, a.den * b.den }; }
    friend constexpr Rational operator- (Rational a, Rational b) { return { a.num * b.den - b.num * a.den, a.den * b.den }; }
    friend constexpr Rational operator* (Rational a, Rational b) { return { a.num * b.num, a.den * b.den }; }
    friend constexpr Rational operator/ (Rational a, Rational b) { return { a.num * b.den, a.den * b.num }; }

    friend constexpr bool operator== (Rational a, Rational b) { return a.num == b.num && a.den == b.den; }
    friend constexpr bool operator!= (Rational a, Rational b) { return ! (a == b); }
    friend constexpr bool operator< (Rational a, Rational b) { return a.num * b.den < b.num * a.den; }
    friend constexpr bool operator<= (Rational a, Rational b) { return ! (b < a); }
    friend constexpr bool operator> (Rational a, Rational b) { return b < a; }
    friend constexpr bool operator>= (Rational a, Rational b) { return ! (a < b); }
};

} // namespace hz
