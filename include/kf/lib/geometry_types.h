#ifndef KF_LIB_GEOMETRY_TYPES_H
#define KF_LIB_GEOMETRY_TYPES_H

#include <kf/lib/types.h>

#include <array>
#include <cstddef>
#include <type_traits>

// Fixed-point value layouts also used by the original resource formats. These
// types carry values only; portable rendering has no implicit register state.
struct MATRIX { std::array<std::array<s16, 3>, 3> m; std::array<s32, 3> t; };
struct SVECTOR;

// Constructed values initialize unused layout padding; arithmetic changes XYZ.
// Keep default construction and copying trivial for resource and union storage.
struct VECTOR {
    s32 vx, vy, vz, pad;

    VECTOR() = default;
    constexpr VECTOR(s32 x, s32 y, s32 z, s32 padding = 0)
        : vx(x), vy(y), vz(z), pad(padding) {}

    constexpr SVECTOR narrowed() const;

    constexpr VECTOR &operator+=(const VECTOR &other)
    {
        vx += other.vx;
        vy += other.vy;
        vz += other.vz;
        return *this;
    }

    constexpr VECTOR &operator+=(const SVECTOR &other);
};

struct SVECTOR {
    s16 vx, vy, vz, pad;

    SVECTOR() = default;
    constexpr SVECTOR(s16 x, s16 y, s16 z, s16 padding = 0)
        : vx(x), vy(y), vz(z), pad(padding) {}

    constexpr VECTOR widened() const { return {vx, vy, vz}; }

    constexpr SVECTOR &operator+=(const SVECTOR &other)
    {
        vx += other.vx;
        vy += other.vy;
        vz += other.vz;
        return *this;
    }
};

// Width conversion keeps the original signed narrowing, not saturation.
constexpr SVECTOR VECTOR::narrowed() const
{
    return {static_cast<s16>(vx), static_cast<s16>(vy), static_cast<s16>(vz)};
}

constexpr VECTOR &VECTOR::operator+=(const SVECTOR &other)
{
    vx += other.vx;
    vy += other.vy;
    vz += other.vz;
    return *this;
}

struct CVECTOR { u8 r, g, b, cd; };
struct DVECTOR { s16 vx, vy; };
static_assert(sizeof(SVECTOR) == 8 && sizeof(VECTOR) == 16 && sizeof(MATRIX) == 32);
static_assert(offsetof(SVECTOR, pad) == 6 && offsetof(VECTOR, pad) == 12);
static_assert(std::is_standard_layout_v<VECTOR> && std::is_standard_layout_v<SVECTOR>);
static_assert(std::is_trivially_copyable_v<VECTOR> && std::is_trivially_copyable_v<SVECTOR>);
static_assert(std::is_trivially_default_constructible_v<VECTOR>
    && std::is_trivially_default_constructible_v<SVECTOR>);

#endif // KF_LIB_GEOMETRY_TYPES_H
