#ifndef KF_LIB_MATH_H
#define KF_LIB_MATH_H

#include <kf/lib/fixed_math.h>
#include <kf/lib/geometry_types.h>
#include <kf/lib/types.h>

inline constexpr int KF_FIXED4_BITS = 4;
inline constexpr int KF_FIXED6_BITS = 6;
inline constexpr int KF_FIXED7_BITS = 7;
inline constexpr int KF_FIXED7_ONE = 128;
inline constexpr int KF_FIXED8_BITS = 8;
inline constexpr int KF_FIXED11_BITS = 11;
inline constexpr int KF_FIXED12_BITS = 12;
inline constexpr int KF_FIXED12_ONE = 0x1000;
inline constexpr int KF_ANGLE_EIGHTH_TURN = 0x200;
inline constexpr int KF_ANGLE_QUARTER_TURN = 0x400;
inline constexpr int KF_ANGLE_HALF_TURN = 0x800;
inline constexpr int KF_ANGLE_THREE_QUARTER_TURN = 0xc00;
inline constexpr int KF_ANGLE_FULL_TURN = 0x1000;
inline constexpr int KF_ANGLE_WRAP_MASK = 0xfff;
inline constexpr int KF_LENGTH_SQUARE_DOWNSHIFT = 3;

inline constexpr int KF_RANDOM_ANGLE_SHIFT = 3;

inline constexpr int KF_CONE_SEARCH_INITIAL_ANGLE_ERROR = 30000;

struct KfVecXZs {
    s16 x;
    s16 z;
};

struct KfVecXZ {
    s32 x;
    s32 z;
};

struct KfVec3s {
    s16 x;
    s16 y;
    s16 z;
};

struct KfEulerAngles {
    s16 x;
    s16 y;
    s16 z;
};

typedef union KfRotation {
    SVECTOR vector;
    struct KfEulerAngles angles;
} KfRotation;

static inline s16 angle_error_magnitude(s16 difference)
{
    s16 folded;
    difference &= KF_ANGLE_WRAP_MASK;
    folded = difference;
    if (difference > KF_ANGLE_HALF_TURN) {
        folded = KF_ANGLE_FULL_TURN - difference;
    }
    return folded;
}

static inline u32 radial_damage_attenuated_scale(
    s32 distance, u32 radius, u16 falloff, u16 scale)
{
    u16 ratio = (distance << KF_FIXED12_BITS) / radius;
    u16 weight = KF_FIXED12_ONE
        - ((u32)(ratio * (KF_FIXED12_ONE - falloff)) >> KF_FIXED12_BITS);
    return (u32)(scale * weight) >> KF_FIXED12_BITS;
}

// World lengths discard three low coordinate bits before squaring, then use
// the quantized integer lookup. Do not substitute floating-point sqrt/hypot.
extern s32 fixed_vector2_length(s32 x, s32 y);
extern s32 fixed_vector3_length(s32 x, s32 y, s32 z);

inline KfVecXZ vector_yaw_probe_xz(const VECTOR &position, s16 yaw, s32 reach)
{
    // Subtract after the signed shift: moving the minus inside changes rounding.
    return {
        position.vx - ((kf::angle_sine(yaw) * reach) >> KF_FIXED12_BITS),
        position.vz + ((kf::angle_cosine(yaw) * reach) >> KF_FIXED12_BITS)
    };
}

extern s16 angle_approach(s16 current, s16 target, s32 step);
extern bool angle_mod_delta_le_half_turn(int lhs, int rhs);
extern s16 angle_shortest_delta(s32 first, s32 second);
extern void angle_to_forward_xz(s16 angle, struct KfVecXZs *direction);
extern bool angle_within_tolerance(int lhs, int rhs, s16 tolerance);
extern void matrix_interpolate(
    const MATRIX *from, const MATRIX *to, MATRIX *output, s32 blend);
extern void matrix_set_rotation_x(s16 angle, MATRIX *matrix);
extern void matrix_set_rotation_y(s16 angle, MATRIX *matrix);
extern void matrix_set_rotation_z(s16 angle, MATRIX *matrix);
extern void matrix_set_rotation_yxz(
    const struct KfEulerAngles *angles, MATRIX *matrix);
extern void pitch_yaw_to_forward_vector(
    const struct KfEulerAngles *angles, SVECTOR *direction);
extern void vector2s_scale_shift11(s16 scale, struct KfVecXZs *vector);
extern void vector2s_scale_shift12(s16 scale, s16 *vector);
extern void vector3i_add_xz(
    VECTOR *destination, const struct KfVecXZs *delta);
extern void vector3s_scale_shift12(s16 scale, SVECTOR *vector);
extern void vector3s_scale_shift12_alt(s16 scale, s16 *vector);
extern s32 vector_xz_to_angle(s32 x, s32 z);

extern void color_lerp_cvector(const CVECTOR *from, const CVECTOR *to, CVECTOR *output, s32 blend);
extern u16 color_lerp_rgb555(u16 from, u16 to, s32 blend);

#endif // KF_LIB_MATH_H
