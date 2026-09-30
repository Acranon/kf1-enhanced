#include <kf/platform/prelude.h>
#include <kf/lib/math.h>

void pitch_yaw_to_forward_vector(const struct KfEulerAngles *angles, SVECTOR *direction)
{
    MATRIX pitch_matrix;
    MATRIX yaw_matrix;
    SVECTOR source;
    VECTOR result;

    source = {0, 0, KF_FIXED12_ONE};
    matrix_set_rotation_x(-angles->x & KF_ANGLE_WRAP_MASK, &pitch_matrix);
    result = kf::matrix_apply_rotation(pitch_matrix, source);
    source = result.narrowed();
    matrix_set_rotation_y(angles->y, &yaw_matrix);
    result = kf::matrix_apply_rotation(yaw_matrix, source);
    *direction = result.narrowed();
}

void vector2s_scale_shift11(s16 scale, struct KfVecXZs *vector)
{
    s32 x = vector->x * scale;
    s32 z = vector->z * scale;

    vector->x = x >> KF_FIXED11_BITS;
    vector->z = z >> KF_FIXED11_BITS;
}

void vector3s_scale_shift12(s16 scale, SVECTOR *vector)
{
    s32 x = vector->vx * scale;
    s32 y = vector->vy * scale;
    s32 z = vector->vz * scale;

    *vector = VECTOR{x >> KF_FIXED12_BITS, y >> KF_FIXED12_BITS, z >> KF_FIXED12_BITS}.narrowed();
}

void vector2s_scale_shift12(s16 scale, s16 *vector)
{
    s32 x = vector[0] * scale;
    s32 y = vector[1] * scale;

    vector[0] = x >> KF_FIXED12_BITS;
    vector[1] = y >> KF_FIXED12_BITS;
}

void vector3s_scale_shift12_alt(s16 scale, s16 *vector)
{
    s32 x = vector[0] * scale;
    s32 y = vector[1] * scale;
    s32 z = vector[2] * scale;

    vector[0] = x >> KF_FIXED12_BITS;
    vector[1] = y >> KF_FIXED12_BITS;
    vector[2] = z >> KF_FIXED12_BITS;
}

void vector3i_add_xz(VECTOR *destination, const struct KfVecXZs *delta)
{
    destination->vx += delta->x;
    destination->vz += delta->z;
}

bool angle_within_tolerance(int lhs, int rhs, s16 tolerance)
{
    int delta = (lhs - rhs) & KF_ANGLE_WRAP_MASK;

    return delta <= tolerance || KF_ANGLE_FULL_TURN - tolerance <= delta;
}

bool angle_mod_delta_le_half_turn(int lhs, int rhs)
{
    return ((lhs - rhs) & KF_ANGLE_WRAP_MASK) < (KF_ANGLE_HALF_TURN + 1);
}

s32 vector_xz_to_angle(s32 x, s32 z)
{
    if (z > 0) {
        return kf::fixed_arctangent((x << KF_FIXED12_BITS) / z) + KF_ANGLE_HALF_TURN;
    }
    if (z < 0) {
        return kf::fixed_arctangent((x << KF_FIXED12_BITS) / z) & KF_ANGLE_WRAP_MASK;
    }
    if (x > 0) {
        return KF_ANGLE_THREE_QUARTER_TURN;
    }
    return KF_ANGLE_QUARTER_TURN;
}

s32 fixed_vector2_length(s32 x, s32 y)
{
    x >>= KF_LENGTH_SQUARE_DOWNSHIFT;
    y >>= KF_LENGTH_SQUARE_DOWNSHIFT;
    return kf::length_square_root(x * x + y * y) << KF_LENGTH_SQUARE_DOWNSHIFT;
}

s32 fixed_vector3_length(s32 x, s32 y, s32 z)
{
    x >>= KF_LENGTH_SQUARE_DOWNSHIFT;
    y >>= KF_LENGTH_SQUARE_DOWNSHIFT;
    z >>= KF_LENGTH_SQUARE_DOWNSHIFT;
    return kf::length_square_root(x * x + y * y + z * z) << KF_LENGTH_SQUARE_DOWNSHIFT;
}

s16 angle_shortest_delta(s32 first, s32 second)
{
    s32 difference;
    s16 signed_difference;

    first &= KF_ANGLE_WRAP_MASK;
    second &= KF_ANGLE_WRAP_MASK;
    difference = second - first;
    signed_difference = difference;
    if (signed_difference >= KF_ANGLE_HALF_TURN) {
        return difference - KF_ANGLE_FULL_TURN;
    }
    if (signed_difference < -KF_ANGLE_HALF_TURN + 1) {
        return difference + KF_ANGLE_FULL_TURN;
    }
    return signed_difference;
}
