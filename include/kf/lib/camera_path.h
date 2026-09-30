#ifndef KF_LIB_CAMERA_PATH_H
#define KF_LIB_CAMERA_PATH_H

#include <kf/lib/geometry_types.h>

#include <span>

enum {
    KF_CAMERA_PATH_END_X = -1,
    KF_CAMERA_PATH_FINISHED = -1
};

typedef struct KfCameraPathPoint {
    VECTOR position;
    SVECTOR rotation;
    s16 speed;
    s16 unknown_1a;
} KfCameraPathPoint;

typedef struct KfCameraPathState {
    std::span<const KfCameraPathPoint> points;
    VECTOR position;
    SVECTOR rotation;
    VECTOR position_fixed;
    VECTOR rotation_fixed;
    VECTOR position_delta;
    VECTOR rotation_delta;
    s16 point_index;
    s16 unknown_5e;
    s32 frames_remaining;
} KfCameraPathState;

extern void camera_path_prepare_segment(KfCameraPathState *path, const KfCameraPathPoint *point);
extern void camera_path_publish_fixed(KfCameraPathState *path);
extern void camera_path_advance_pose(KfCameraPathState *path, s32 y_offset);

#endif // KF_LIB_CAMERA_PATH_H
