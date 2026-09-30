#ifndef KF_LIB_FIXED_MATH_H
#define KF_LIB_FIXED_MATH_H

#include <kf/lib/geometry_types.h>

namespace kf {
// Angles are signed 4096-unit turns; results use signed Q12 amplitude.
s32 angle_sine(s32 angle);
s32 angle_cosine(s32 angle);
s32 fixed_arctangent(s32 ratio_q12);
// Retains the original nonnegative signed-input approximation. High-bit inputs
// are defined as unsigned lengths instead of indexing outside the old table.
s32 length_square_root(u32 squared_length);

// Q12 rotation operations never read or change translation. Output may alias
// either input when composing matrices.
void matrix_multiply_rotation(const MATRIX &left, const MATRIX &right, MATRIX &output);
void matrix_scale_axes(MATRIX &matrix, const VECTOR &scale);
VECTOR matrix_apply_rotation(const MATRIX &matrix, const SVECTOR &vector);
void matrix_set_rotation_xyz(const SVECTOR &angles, MATRIX &matrix);
}

#endif // KF_LIB_FIXED_MATH_H
