#ifndef KF_OPEN_CAMERA_PATH_H
#define KF_OPEN_CAMERA_PATH_H

#include <kf/lib/map.h>

#include <span>

extern KfCameraPathState opening_camera_path_state;

extern void opening_camera_path_compute_segment(void);
extern void opening_camera_path_begin(std::span<const KfCameraPathPoint> points);
extern void opening_camera_path_step(s32 y_offset);

#endif // KF_OPEN_CAMERA_PATH_H
