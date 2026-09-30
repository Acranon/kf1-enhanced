#include <kf/platform/prelude.h>
#include <kf/lib/camera_path.h>
#include <kf/lib/math.h>

void camera_path_prepare_segment(KfCameraPathState *path, const KfCameraPathPoint *point)
{
    if (point->position.vx == KF_CAMERA_PATH_END_X) {
        path->frames_remaining = KF_CAMERA_PATH_FINISHED;
        return;
    }
    const s32 delta_x = point->position.vx - path->position.vx;
    const s32 delta_y = point->position.vy - path->position.vy;
    const s32 delta_z = point->position.vz - path->position.vz;
    const s32 distance = fixed_vector3_length(delta_x, delta_y, delta_z);
    path->position_delta = {
        (delta_x << KF_FIXED4_BITS) * point->speed / distance,
        (delta_y << KF_FIXED4_BITS) * point->speed / distance,
        (delta_z << KF_FIXED4_BITS) * point->speed / distance};
    path->frames_remaining = distance / point->speed;
    const s32 pitch_delta = angle_shortest_delta(path->rotation.vx, point->rotation.vx);
    const s32 yaw_delta = angle_shortest_delta(path->rotation.vy, point->rotation.vy);
    const s16 roll_delta = angle_shortest_delta(path->rotation.vz, point->rotation.vz);
    path->rotation_delta = {
        (pitch_delta << KF_FIXED4_BITS) / path->frames_remaining,
        (yaw_delta << KF_FIXED4_BITS) / path->frames_remaining,
        (roll_delta << KF_FIXED4_BITS) / path->frames_remaining};
}

void camera_path_publish_fixed(KfCameraPathState *path)
{
    path->position_fixed = {
        path->position.vx << KF_FIXED4_BITS,
        path->position.vy << KF_FIXED4_BITS,
        path->position.vz << KF_FIXED4_BITS};
    path->rotation_fixed = {
        path->rotation.vx << KF_FIXED4_BITS,
        path->rotation.vy << KF_FIXED4_BITS,
        path->rotation.vz << KF_FIXED4_BITS};
}

void camera_path_advance_pose(KfCameraPathState *path, s32 y_offset)
{
    path->position_fixed += path->position_delta;
    path->rotation_fixed += path->rotation_delta;
    path->position = {
        path->position_fixed.vx >> KF_FIXED4_BITS,
        (path->position_fixed.vy >> KF_FIXED4_BITS) + y_offset,
        path->position_fixed.vz >> KF_FIXED4_BITS};
    path->rotation = VECTOR{
        (path->rotation_fixed.vx >> KF_FIXED4_BITS) & KF_ANGLE_WRAP_MASK,
        (path->rotation_fixed.vy >> KF_FIXED4_BITS) & KF_ANGLE_WRAP_MASK,
        (path->rotation_fixed.vz >> KF_FIXED4_BITS) & KF_ANGLE_WRAP_MASK}.narrowed();
}
